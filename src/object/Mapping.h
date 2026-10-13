#pragma once

#include "MainConfig.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using namespace std;

// mapping
#define T_UNDEFINED 0x0
#define T_NUMBER    0x2
#define T_STRING    0x4
#define T_MAPPING   0x8
#define T_ARRAY     0x10

#define STRING_MALLOC   0x1 /* has a length an ref count */
#define STRING_HASHED   0x2 /* is in the shared string table */
#define STRING_CONSTANT 0

// xpjson 的取值类型；仅在此前向声明，避免把 xpjson.hpp（含 snprintf 等宏）
// 带入本头文件的所有包含者。实际使用只在 Mapping.cpp。
namespace JSON {
template <class char_t>
class ValueT;
}

/**
 * @brief 字符串阵列，保持插入顺序并确保唯一性。
 */
class CVector {
public:
    typedef vector<string> VECTOR_STR;
    typedef VECTOR_STR::iterator iterator;

    long load_buffer(const char* buffer);

    // 序列化为 JSON 数组文本 (["a","b"])；定义见 Mapping.cpp（依赖 xpjson）。
    const char* get_buffer();

    iterator begin() { return m_array.begin(); };

    iterator end() { return m_array.end(); };

    void append(const string& t)
    {
        if (m_set.insert(t).second)  // 唯一性：索引命中则不重复插入
            m_array.push_back(t);
    };

    void remove(const string& t)
    {
        if (m_set.erase(t))  // 索引命中才去数组中删除
        {
            iterator p = std::find(m_array.begin(), m_array.end(), t);
            if (p != m_array.end()) m_array.erase(p);
        }
    };

    iterator find(const string& t)
    {
        return std::find(m_array.begin(), m_array.end(), t);
    };

    const char* operator[](unsigned int pos)
    {
        if (pos >= m_array.size()) return "";  // 越界返回空串
        return m_array[pos].c_str();
    };

    void clear()
    {
        m_array.clear();
        m_set.clear();
    };

    long count() { return (long)m_array.size(); }

    CVector() {};
    ~CVector() { clear(); };

protected:
    VECTOR_STR m_array;           // 有序数组
    unordered_set<string> m_set;  // 唯一性索引（与 m_Array 同步维护）
    string m_string;              // get_buffer 输出缓冲
};

/**
 * @file Mapping.h
 * @brief CMapping / CVector — 游戏核心键值存储与序列化容器。
 *
 * @section sec_overview 概述
 *
 * CMapping 是一个以 **字符串为键** 的异构映射表，每个值可以是以下四种类型之一：
 *  - T_NUMBER  (LONG 整数)
 *  - T_STRING  (以 \\0 结尾的 C 字符串，拥有所有权)
 *  - T_MAPPING (嵌套的 CMapping 子映射，拥有所有权)
 *  - T_ARRAY   (CVector 字符串阵列，拥有所有权)
 *
 * 设计目标：
 *  - **简单性**：对外只暴露 set / query / del / add 等基本 CRUD 操作。
 *  - **路径寻址**：键名支持 '/' 分隔的多级路径（如 "family/family_name"），
 *    自动遍历或创建中间映射节点。
 *  - **可序列化**：支持 GetBuffer() / LoadBuffer() 与磁盘格式互转，
 *    格式为 ([\"key\":value,]) 的自描述文本协议。
 *  - **深拷贝安全**：copyMapping() 递归复制整个子树，无共享状态。
 *
 * @section sec_value_types 值类型系统
 *
 * 每个键值对以 svalue_t 存储，type 字段标识实际类型：
 * | type       | u 字段      | 说明                          |
 * |------------|-------------|-------------------------------|
 * | T_NUMBER   | u.number    | LONG 整数                     |
 * | T_STRING   | u.string    | 拥有所有权的 C 字符串          |
 * | T_MAPPING  | u.map       | 拥有所有权的子映射             |
 * | T_ARRAY    | u.arr       | 拥有所有权的字符串阵列          |
 *
 * svalue_t 为 move-only，禁止拷贝。
 *
 * **所有权规则**：CMapping 拥有其存储的所有值的资源。当键被覆盖或删除时，
 * free_svalue() 会递归释放旧值的内存（包括嵌套映射和数组）。
 * 调用方不应 delete 通过 querymap / queryvec 获取的指针。
 *
 * @section sec_nested_keys 路径寻址（嵌套键）
 *
 * 所有 set / query / del / add 操作均支持 '/' 分隔的多级路径：
 *  - set("a/b/c", 42)  → 创建 a (映射) → b (映射) → c = 42
 *  - query("a/b/c")    → 返回 42
 *  - querymap("a/b")   → 返回 b 对应的子映射
 *  - del("a/b/c")      → 删除 c，a 和 b 保留（即使为空）
 *
 * 路径解析由内部 get_master_map() 实现，对调用方透明。
 *
 * @section sec_serialization 序列化格式
 *
 * get_buffer() 将整个映射序列化为标准 JSON（由 xpjson 生成）：
 * @code
 *   {"key1":value1,"key2":value2}    // 顶层映射
 *   ["a","b"]                         // 数组 (CVector)
 * @endcode
 * 值的编码规则：
 *  - 字符串：双引号包裹，按 JSON 规则转义（\\\" 、\\\\ 、\\/ 、控制字符 \\n 等
 *    无损保留）；高位字节（游戏内部 GBK 文本）原样透传。
 *  - 整数：十进制文本，支持负数（如 -5）。
 *  - 映射：递归嵌套 {...} 。
 *  - 数组：["elem1","elem2"] 。
 *
 * load_buffer() 为逆操作，从 JSON 文本恢复映射内容。
 * 两者共同构成数据库存盘 / 读档的基础。存盘时 JSON 文本经 sql_escape_literal
 * 处理单引号后写入 blob（连接设 NO_BACKSLASH_ESCAPES，反斜杠原样存储）。
 *
 * @section sec_memory 内存管理
 *
 * - svalue_t 为 move-only。字符串、子映射、数组均拥有所有权，
 *   赋值/删除旧值时自动释放，无需手工 delete。
 * - get_buffer() 内部维护 m_buffer（std::string，按需扩容），
 *   调用方可直接使用返回的指针，下次 get_buffer() 会覆盖。
 * - 析构时自动调用 free_map() 清空所有键值对。
 *
 * @section sec_thread_safety 线程安全
 *
 * CMapping 本身 **不是线程安全的**。多线程并发读写同一实例需要外部同步。
 *
 * @section sec_example 使用示例
 * @code
 *   CMapping m;
 *   m.set("name", "张三");
 *   m.set("age", 25L);
 *   m.set("skills/force", 100L);     // 嵌套键
 *
 *   const char* name = m.querystr("name");      // "张三"
 *   long age = m.query("age");                   // 25
 *   long force = m.query("skills/force");        // 100
 *
 *   m.add("age", 5L);                            // age 变为 30
 *   m.del("skills/force");                       // 删除叶子节点
 *
 *   const char* buf = m.GetBuffer();             // 序列化
 *   CMapping m2;
 *   m2.LoadBuffer(buf);                          // 反序列化
 * @endcode
 */
class CMapping {
public:
    // 值类型：number 为内联整数；string/map/arr 拥有所有权。
    // svalue_t 为 move-only，禁止拷贝。
    typedef struct svalue_s {
        int type;
        int subtype;

        struct {
            LONG number = 0;
            std::unique_ptr<char[]> string;
            std::unique_ptr<CMapping> map;
            std::unique_ptr<CVector> arr;
        } u;

        svalue_s() : type(T_NUMBER), subtype(0) {}
        svalue_s(const svalue_s&)                = delete;
        svalue_s& operator=(const svalue_s&)     = delete;
        svalue_s(svalue_s&&) noexcept            = default;
        svalue_s& operator=(svalue_s&&) noexcept = default;
    } svalue_t;

    // ----- 公共接口 -----
    CMapping();
    virtual ~CMapping();

    void set(const char* key, const char* val);
    void set(const char* key, long val);
    void set(const char* key, CMapping& mapping);
    void set(const char* key, CVector& v);
    void del(const char* key);

    long query(const char* key);
    const char* querystr(const char* key);
    CMapping* querymap(const char* key);
    CVector* queryvec(const char* key);
    long add(const char* key, long n);
    int undefinedp(const char* key);
    int get_count();
    void keys(CVector& v);
    void values(CVector& v);

    void copy_mapping(CMapping* m);
    void free_map();
    void clear();

    const char* get_buffer();
    long load_buffer(const char* buffer);

    svalue_t* find_string_in_mapping(const char* p);

    LONG& operator[](const char* key);

    // 核心存储：std::string 键 -> svalue_t 值。
    // svalue_t 的资源（字符串/嵌套映射/数组）由 free_svalue
    // 在删除/覆盖时显式释放。
    std::unordered_map<std::string, svalue_t> m_map;

    std::string m_buffer;  // get_buffer 输出缓冲（JSON 文本）

private:
    // ----- 内部辅助 -----
    svalue_t* insert_in_mapping(const char* key);
    CMapping* get_master_map(const char* key, const char** subkey,
                             int insertmap, const char* comm);
    void free_svalue(svalue_t* v);
    static std::unique_ptr<char[]> make_string(const char* value);

    // svalue_t <-> xpjson 值的互转，供 get_buffer / load_buffer 递归使用。
    // 参数用前向声明的 JSON::ValueT<char>&，避免把头文件暴露给 xpjson。
    static void to_json(const svalue_t& v, JSON::ValueT<char>& out);
    static void to_json_map(const CMapping& map, JSON::ValueT<char>& out);
    // 从一个 JSON object 值填充本映射（递归处理嵌套 object/array）。
    void from_json_object(JSON::ValueT<char>& val);

    LONG m_value;
};

// 未命中哨兵（定义于 Mapping.cpp）。find_string_in_mapping
// 未命中时返回其地址，调用方以 nvalue != &const0u 判断命中。
extern CMapping::svalue_t const0u;
