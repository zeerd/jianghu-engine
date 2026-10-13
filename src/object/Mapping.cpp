/**
 * @file Mapping.cpp
 * @brief implementation of the CMapping class. */

#include "Mapping.h"
#include "stdio.h"

#include "Global.h"

#include <string.h>
#include "xpjson.hpp"
// xpjson 在 _WIN32 下 #define snprintf _snprintf；本文件序列化改用
// std::string， 关闭宏以免外泄到后续（乃至经此 TU 传递的）代码。
#undef snprintf
using namespace std;

// 未命中哨兵：find_string_in_mapping 未命中时返回 &const0u（默认构造为
// T_NUMBER/0）
CMapping::svalue_t const0u;

void printf_all_mappings() {}

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------
CMapping::CMapping() { m_value = 0; }

CMapping::~CMapping() { free_map(); }

void CMapping::free_map()
{
    for (std::unordered_map<std::string, svalue_t>::iterator it = m_map.begin();
         it != m_map.end(); ++it) {
        free_svalue(&it->second);
    }
    m_map.clear();
}

void CMapping::clear() { free_map(); }

void CMapping::copy_mapping(CMapping* m)
{
    if (!m || m == this) return;

    free_map();

    for (auto it = m->m_map.begin(); it != m->m_map.end(); ++it) {
        svalue_t& src = it->second;
        svalue_t sv;
        sv.type    = src.type;
        sv.subtype = src.subtype;

        switch (src.type) {
            case T_STRING:
                sv.u.string = make_string(src.u.string.get());
                break;
            case T_MAPPING: {
                auto nm = std::make_unique<CMapping>();
                nm->copy_mapping(src.u.map.get());
                sv.u.map = std::move(nm);
            } break;
            case T_ARRAY: {
                auto nv  = std::make_unique<CVector>();
                (*nv)    = *(src.u.arr);
                sv.u.arr = std::move(nv);
            } break;
            default:
                sv.u.number = src.u.number;
                break;
        }

        m_map.insert(std::make_pair(it->first, std::move(sv)));
    }
}

std::unique_ptr<char[]> CMapping::make_string(const char* value)
{
    if (!value) return nullptr;
    int len  = (int)strlen(value);
    auto ptr = std::make_unique<char[]>(len + 1);
    memcpy(ptr.get(), value, len + 1);
    return ptr;
}

void CMapping::free_svalue(svalue_t* v)
{
    if (!v) return;

    // 此处仅需重置对应字段与 type。
    switch (v->type) {
        case T_STRING:
            v->u.string.reset();
            break;
        case T_ARRAY:
            v->u.arr.reset();
            break;
        case T_MAPPING:
            v->u.map.reset();
            break;
        default:
            v->u.number = 0;
            break;
    }
    v->subtype = 0;
    v->type    = T_UNDEFINED;
}

CMapping::svalue_t* CMapping::find_string_in_mapping(const char* p)
{
    if (!p || !p[0]) return &const0u;

    std::unordered_map<std::string, svalue_t>::iterator it = m_map.find(p);
    if (it == m_map.end()) return &const0u;
    return &it->second;
}

CMapping::svalue_t* CMapping::insert_in_mapping(const char* key)
{
    if (!key || !key[0]) return &const0u;

    std::unordered_map<std::string, svalue_t>::iterator it = m_map.find(key);
    if (it != m_map.end()) {
        free_svalue(&it->second);  // 覆盖旧值（与旧
                                   // find_for_insert(doTheFree=1) 语义一致）
        return &it->second;
    }

    svalue_t sv;  // 默认构造 T_NUMBER/0
    it = m_map.insert(std::make_pair(std::string(key), std::move(sv))).first;
    return &it->second;
}

// 需要优化的代码
CMapping* CMapping::get_master_map(const char* key, const char** subkey,
                                   int insertmap, const char* comm)
{
    // 分析是否有 形如：mask/tt/u2  ->map:mask -> map:tt ->query("u2")

    (*subkey) = 0;
    if (!key || !key[0]) return insertmap ? this : 0;

    const char* slash = strchr(key, '/');
    if (slash) {
        (*subkey) = slash + 1;
        std::string seg(key, slash - key);  // 第一段（无 255 字节限制）

        svalue_t* nvalue = find_string_in_mapping(seg.c_str());
        if (nvalue != &const0u && nvalue->type == T_MAPPING)
            return nvalue->u.map.get();

        if (insertmap) {
            svalue_t* s;
            if (nvalue != &const0u) {
                s = nvalue;
                free_svalue(s);
            }
            else
                s = insert_in_mapping(seg.c_str());

            s->type  = T_MAPPING;
            s->u.map = std::make_unique<CMapping>();
            return s->u.map.get();
        }

        return 0;
    }

    return this;
}

void CMapping::set(const char* key, const char* val)
{
    if (!key || !key[0]) {
        LOG_ERROR("set: key is null!");
        return;
    }
    if (!val) {
        LOG_ERROR("set: val is null!");
        return;
    }

    const char* subkey;
    CMapping* map = this;

    do {
        map = map->get_master_map(key, &subkey, 1, "set");
    } while (subkey && (key = subkey));

    svalue_t* s = map->insert_in_mapping(key);
    s->type     = T_STRING;
    s->subtype  = STRING_MALLOC;
    s->u.string = make_string(val);
}

void CMapping::set(const char* key, long val)
{
    if (!key || !key[0]) {
        LOG_ERROR("set: key is null!");
        return;
    }

    const char* subkey;
    CMapping* map = this;

    do {
        map = map->get_master_map(key, &subkey, 1, "set");
    } while (subkey && (key = subkey));

    svalue_t* s = map->insert_in_mapping(key);
    s->type     = T_NUMBER;
    s->u.number = val;
}

void CMapping::set(const char* key, CMapping& mapping)
{
    if (!key || !key[0]) {
        LOG_ERROR("set: key is null!");
        return;
    }

    const char* subkey;
    CMapping* map = this;

    do {
        map = map->get_master_map(key, &subkey, 1, "set");
    } while (subkey && (key = subkey));

    svalue_t* s = map->insert_in_mapping(key);
    s->type     = T_MAPPING;

    auto m = std::make_unique<CMapping>();
    m->copy_mapping(&mapping);
    s->u.map = std::move(m);
}

void CMapping::set(const char* key, CVector& v)
{
    if (!key || !key[0]) {
        LOG_ERROR("set: key is null!");
        return;
    }

    const char* subkey;
    CMapping* map = this;

    do {
        map = map->get_master_map(key, &subkey, 1, "set");
    } while (subkey && (key = subkey));

    svalue_t* s = map->insert_in_mapping(key);
    s->type     = T_ARRAY;

    auto vec = std::make_unique<CVector>();
    (*vec)   = v;
    s->u.arr = std::move(vec);
}

void CMapping::del(const char* key)
{
    if (!key || !key[0]) {
        LOG_ERROR("del: key is null!");
        return;
    }

    const char* subkey;
    CMapping* map = this;

    do {
        if (!(map = map->get_master_map(key, &subkey, 0, "del"))) return;
    } while (subkey && (key = subkey));

    auto it = map->m_map.find(key);
    if (it != map->m_map.end()) {
        free_svalue(&it->second);
        map->m_map.erase(it);
    }
}

long CMapping::query(const char* key)
{
    if (!key || !key[0]) return 0;

    const char* subkey;
    CMapping* map = this;

    do {
        if (!(map = map->get_master_map(key, &subkey, 0, "query"))) return 0;
    } while (subkey && (key = subkey));

    svalue_t* nvalue = map->find_string_in_mapping(key);
    if (nvalue != &const0u && nvalue->type == T_NUMBER) return nvalue->u.number;

    return 0;
}

const char* CMapping::querystr(const char* key)
{
    if (!key || !key[0]) return "";

    const char* subkey;
    CMapping* map = this;

    do {
        if (!(map = map->get_master_map(key, &subkey, 0, "querystr")))
            return "";
    } while (subkey && (key = subkey));

    svalue_t* nvalue = map->find_string_in_mapping(key);
    if (nvalue != &const0u && nvalue->type == T_STRING)
        return nvalue->u.string ? nvalue->u.string.get() : "";

    return "";
}

CMapping* CMapping::querymap(const char* key)
{
    if (!key || !key[0]) return 0;

    const char* subkey;
    CMapping* map = this;

    do {
        if (!(map = map->get_master_map(key, &subkey, 0, "querymap"))) return 0;
    } while (subkey && (key = subkey));

    svalue_t* nvalue = map->find_string_in_mapping(key);
    if (nvalue != &const0u && nvalue->type == T_MAPPING)
        return nvalue->u.map.get();

    return 0;
}

CVector* CMapping::queryvec(const char* key)
{
    if (!key || !key[0]) return 0;

    const char* subkey;
    CMapping* map = this;

    do {
        if (!(map = map->get_master_map(key, &subkey, 0, "queryvec"))) return 0;
    } while (subkey && (key = subkey));

    svalue_t* nvalue = map->find_string_in_mapping(key);
    if (nvalue != &const0u && nvalue->type == T_ARRAY)
        return nvalue->u.arr.get();

    return 0;
}

long CMapping::add(const char* key, long value)
{
    if (!key || !key[0]) return 0;

    const char* subkey;
    CMapping* map = this;

    do {
        map = map->get_master_map(key, &subkey, 1, "add");
    } while (subkey && (key = subkey));

    svalue_t* nvalue = map->find_string_in_mapping(key);
    if (nvalue != &const0u && nvalue->type == T_NUMBER) {
        nvalue->u.number += value;
        return nvalue->u.number;
    }

    svalue_t* s;
    if (nvalue != &const0u) {
        s = nvalue;
        free_svalue(s);
    }
    else
        s = map->insert_in_mapping(key);

    s->type     = T_NUMBER;
    s->u.number = value;

    return value;
}

int CMapping::undefinedp(const char* key)
{
    if (!key || !key[0]) return 1;

    const char* subkey;
    CMapping* map = this;

    do {
        if (!(map = map->get_master_map(key, &subkey, 0, "undefinedp")))
            return 1;
    } while (subkey && (key = subkey));

    svalue_t* nvalue = map->find_string_in_mapping(key);
    return nvalue == &const0u;
}

int CMapping::get_count() { return (int)m_map.size(); }

void CMapping::keys(CVector& v)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        v.append(it->first);
    }
}

void CMapping::values(CVector& v)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if (it->second.type == T_STRING)
            v.append(it->second.u.string ? it->second.u.string.get() : "");
    }
}

LONG& CMapping::operator[](const char* key)
{
    svalue_t* nvalue = find_string_in_mapping(key);
    if (nvalue != &const0u && nvalue->type == T_NUMBER) return nvalue->u.number;

    m_value = 0;
    return m_value;
}

// svalue_t -> xpjson 值。递归处理嵌套映射与数组。
// out 由调用方以 JSON::Value 的 operator[] 取得（初始为 NIL），
// 赋值/取子结构时由 xpjson 自动定型。
void CMapping::to_json(const svalue_t& v, JSON::ValueT<char>& out)
{
    switch (v.type) {
        case T_STRING:
            out = std::string(v.u.string ? v.u.string.get() : "");
            return;
        case T_NUMBER:
            out = static_cast<int64_t>(v.u.number);
            return;
        case T_MAPPING:
            to_json_map(*v.u.map, out);
            return;
        case T_ARRAY: {
            out.a();  // 空数组也要定型为 ARRAY，序列化为 []（否则会成 null
                      // 而丢键）
            CVector* vec = v.u.arr.get();
            for (long i = 0; i < vec->count(); ++i)
                out[static_cast<int>(i)]
                    = std::string((*vec)[static_cast<unsigned>(i)]);
            return;
        }
        default:
            // T_UNDEFINED 等不会出现在持久化数据中；留作 JSON null。
            return;
    }
}

// CMapping -> JSON object（键为 m_map 的字符串键，值递归转换）。
void CMapping::to_json_map(const CMapping& map, JSON::ValueT<char>& out)
{
    out.o();  // 空映射也定型为 OBJECT，序列化为 {}
    for (std::unordered_map<std::string, svalue_t>::const_iterator it
         = map.m_map.begin();
         it != map.m_map.end(); ++it) {
        to_json(it->second, out[it->first.c_str()]);
    }
}

const char* CMapping::get_buffer()
{
    JSON::Value root;
    to_json_map(*this, root);
    m_buffer.clear();
    root.write(m_buffer);
    return m_buffer.c_str();
}

// 从一个 JSON object 值填充本映射。嵌套 object -> 子映射，
// 嵌套 array -> CVector（仅收录字符串元素，append 维持有序去重）。
// null/布尔/浮点等非本格式产出的类型跳过该键（视为未定义）。
void CMapping::from_json_object(JSON::ValueT<char>& val)
{
    JSON::Object& obj = val.o();
    for (JSON::Object::iterator it = obj.begin(); it != obj.end(); ++it) {
        const std::string& key = it->first;
        JSON::Value& child     = it->second;
        switch (child.type()) {
            case JSON::INTEGER: {
                svalue_t* s = insert_in_mapping(key.c_str());
                s->type     = T_NUMBER;
                s->u.number = static_cast<LONG>(child.i());
            } break;
            case JSON::STRING: {
                svalue_t* s = insert_in_mapping(key.c_str());
                s->type     = T_STRING;
                s->subtype  = STRING_MALLOC;
                s->u.string = make_string(child.s().c_str());
            } break;
            case JSON::OBJECT: {
                svalue_t* s = insert_in_mapping(key.c_str());
                s->type     = T_MAPPING;
                s->u.map    = std::make_unique<CMapping>();
                s->u.map->from_json_object(child);
            } break;
            case JSON::ARRAY: {
                svalue_t* s      = insert_in_mapping(key.c_str());
                s->type          = T_ARRAY;
                s->u.arr         = std::make_unique<CVector>();
                JSON::Array& arr = child.a();
                for (JSON::Array::iterator e = arr.begin(); e != arr.end();
                     ++e) {
                    if (e->type() == JSON::STRING) s->u.arr->append(e->s());
                }
            } break;
            default:
                // NIL / BOOLEAN / FLOAT：非本写侧产出的类型，跳过。
                break;
        }
    }
}

long CMapping::load_buffer(const char* buffer)
{
    if (!buffer) return -1;

    free_map();

    try {
        JSON::Value root;
        size_t consumed = root.read(buffer, strlen(buffer));
        if (root.type() != JSON::OBJECT) return -1;
        from_json_object(root);
        return static_cast<long>(consumed);
    } catch (std::exception&) {
        // 非法/截断 JSON：维持既有"返回 -1 表示失败"的契约。
        return -1;
    }
}

// CVector -> JSON 数组文本 (["a","b"])。定义于 cpp 以便使用 xpjson。
const char* CVector::get_buffer()
{
    JSON::Value root;
    root.a();  // 空阵列也定型为 ARRAY，序列化为 []
    for (long i = 0; i < count(); ++i)
        root[static_cast<int>(i)]
            = std::string(m_array[static_cast<size_t>(i)]);
    m_string.clear();
    root.write(m_string);
    return m_string.c_str();
}

long CVector::load_buffer(const char* buffer)
{
    if (!buffer) return -1;

    clear();

    try {
        JSON::Value root;
        size_t consumed = root.read(buffer, strlen(buffer));
        if (root.type() != JSON::ARRAY) return -1;
        JSON::Array& arr = root.a();
        for (JSON::Array::iterator e = arr.begin(); e != arr.end(); ++e) {
            if (e->type() == JSON::STRING) append(e->s());  // 有序去重
        }
        return static_cast<long>(consumed);
    } catch (std::exception&) {
        return -1;
    }
}
