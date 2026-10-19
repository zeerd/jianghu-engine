/**
 * @file LuaEngine.h
 * @brief Lua 5.5 运行时引擎，管理脚本加载、热重载与 call_out 回调。
 */

//---------------------------------------------------------

#pragma once

#include <map>
#include <string>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "Logger.h"

struct lua_State;
class CContainer;

enum LuaScriptType {
    LUA_SCRIPT_NPC = 0,
    LUA_SCRIPT_ITEM,
    LUA_SCRIPT_SKILL,
    LUA_SCRIPT_ROOM,
    LUA_SCRIPT_CONDITION,
    LUA_SCRIPT_DATA,  // 通用数据配置脚本（city_data, chanbanword 等）
};

struct LuaScript {
    std::string file;           // 脚本文件路径
    LuaScriptType type;         // 脚本类型
    int ref;                    // 脚本 table 的 registry 引用
    unsigned long contentHash;  // 文件内容哈希（用于热重载检测）
    time_t mtime;               // 文件最后修改时间（快速过滤用）
    bool loaded;                // 是否已成功加载
    bool hasError;              // 是否有语法/加载错误
    bool fromDefault;           // 是否来自默认脚本目录（不参与热重载）
    std::string errorMsg;       // 最近一次错误信息
    time_t errorTime;           // 最近一次错误时间

    LuaScript()
        : type(LUA_SCRIPT_NPC),
          ref(LUA_NOREF),
          contentHash(0),
          mtime(0),
          loaded(false),
          hasError(false),
          fromDefault(false),
          errorTime(0)
    {
    }
};

// load_root_script 的执行结果：
//   Loaded  — 已在某个脚本根找到并执行完毕；
//   Missing — 所有脚本根都没有该文件（可选脚本的正常缺席）；
//   Failed  — 文件存在但读取/编译/执行失败（已记 ERROR 日志）。
enum class RootScriptStatus { Loaded, Missing, Failed };

/**
 * @brief Lua 运行时引擎，全局单例，管理脚本生命周期与执行环境。
 */
class CLuaEngine {
public:
    static CLuaEngine* instance();

    // 初始化 / 关闭
    // scriptRoot: 用户脚本根目录（优先级高，支持热重载）
    // defaultScriptRoot: 服务器自带默认脚本根目录（可选，不参与热重载）
    bool init(const char* scriptRoot, const char* defaultScriptRoot = NULL);
    void shutdown();

    // 加载脚本：返回脚本 table 的 registry 引用（LUA_NOREF 表示失败）
    // 若脚本已加载且文件已修改，则自动热重载
    int load_script(const char* name, LuaScriptType type);

    // 获取已加载脚本（按类型隔离：同名的 item/skill 脚本各自独立）
    LuaScript* get_script(const char* name, LuaScriptType type);

    // 检查脚本文件是否存在
    bool script_exists(const char* name, LuaScriptType type);

    //---------------------------------------------------------
    // 根脚本加载：直接定义全局函数/表的脚本（damage_msg.lua、
    // data/hooks.lua 这类不走按类型注册表的脚本）
    //---------------------------------------------------------

    // 按 用户脚本根 → 默认脚本根 顺序加载 rel_path（相对根目录，如
    // "damage_msg.lua"、"data/hooks.lua"）。
    // 未提供该文件 → Missing（静默，供可选脚本按需提示）；
    // 文件存在但读取/编译/执行失败 → Failed 并记 ERROR（不回退默认根，
    // 用户脚本坏了必须暴露，不能被默认脚本掩盖）。
    RootScriptStatus load_root_script(const char* rel_path);

    // 获取脚本的 type 字段（用于 Item/Room 选择基类）
    // def 为默认值；具体取值由数据集脚本定义，C++ 不枚举其名称。
    const char* get_script_type(const char* name, LuaScriptType type,
                                const char* def = "item");

    // 获取脚本的任意字符串字段（fieldName），用于选择 C++ 基类。
    // def 为默认值。
    const char* get_script_field(const char* name, LuaScriptType type,
                                 const char* fieldName, const char* def);

    // 手动热重载单个脚本（返回新的 registry 引用）
    // compositeKey: 复合 key（如 "item/hammer"）
    int reload_script(const char* compositeKey);

    // 按 bareName 搜索并重载所有匹配的脚本（供 wizard reload 命令调用）
    // 返回最后重载的 ref（若无匹配返回 LUA_NOREF）
    int reload_script_by_name(const char* bareName);

    // 重载所有已加载脚本
    void reload_all_scripts();

    // 检查所有已加载脚本是否有变化，自动重载（供心跳调用）
    // 返回重载的脚本数量
    int check_for_reload();

    // 加载所有房间脚本并建立 中文地名 -> 脚本名 映射（供 load_room 匹配）
    // 需在 ScanScriptTree 之后调用；可重复调用以重建索引（热重载后刷新）
    void load_room_index();

    // 将房间的中文地名解析为脚本名（拼音文件名，即 m_scripts 的 key）。
    // 若 name 已是脚本名则原样返回；若是中文地名则查 m_roomNameMap 映射；
    // 找不到则原样返回。用于 ROOM 类型脚本的匹配归一化。
    const char* resolve_room_name(const char* name);

    // 按 base_name 前缀解析共享脚本名（动态实例房间机制）。
    // 命中返回脚本名，未命中返回空串。
    const char* resolve_room_prefix(const char* base_name);

    // 读取脚本返回表中的布尔字段（字段存在且为真值时返回 true）。
    bool get_script_bool_field(const char* name, LuaScriptType type,
                               const char* field);

    // 读取脚本返回表中的字符串字段并拷贝到 out（字段缺席或非字符串时
    // out 置空并返回 false）。get_script_field 返回的是已弹出栈的 Lua
    // 内部字符串指针，需要立即取用的场景用本接口。
    bool get_script_str_field(const char* name, LuaScriptType type,
                              const char* field, std::string& out);

    // 获取脚本错误状态（供查询）
    // 返回 true 表示脚本有错误
    bool has_script_error(const char* name, LuaScriptType type);
    // 获取脚本错误信息
    const char* get_script_error(const char* name, LuaScriptType type);
    // 获取有错误的脚本数量
    int get_error_count();

    // 执行脚本中的某个函数（如 create/do_talk 等）
    // 返回 0 表示成功，非 0 表示失败
    int call_function(int scriptRef, const char* funcName, int args,
                      int results);

    //---------------------------------------------------------
    // 统一数据脚本接口（DATA 类型）
    // 封装 Lua 栈操作、UTF-8 转换、热重载，对外仅暴露纯 C++ 接口
    //---------------------------------------------------------

    // 加载 DATA 类型脚本（内部复用 LoadScript 机制，支持热重载）
    // 返回 true 表示加载成功
    bool load_data_script(const char* name);

    // 通用：获取脚本返回表中指定字段的字符串数组
    // 内部处理：LoadScript -> 读取 table -> 遍历数组 -> UTF-8 转 GBK
    bool get_data_array(const char* name, const char* field,
                        std::vector<std::string>& out);

    // 通用：可选数据脚本的字符串数组读取。数据集可以不提供该脚本：
    // 文件缺失视为"未配置"，out 清空返回 true；文件存在但加载失败
    // 仍按 get_data_array 的错误路径返回 false。
    bool get_optional_data_array(const char* name, const char* field,
                                 std::vector<std::string>& out);

    // 通用：获取脚本返回表中指定字段的键值映射
    bool get_data_map(const char* name, const char* field,
                      std::map<std::string, int>& out);

    // 专用：获取城市列表（内部调用 LoadDataScript("city_data") 解析
    // cities/area_to_city）
    bool get_city_list(std::vector<std::string>& out);

    // 专用：获取禁词列表（内部调用 LoadDataScript("chanbanword") 解析
    // banned_words）。禁词表是可选配置：数据集不提供 chanbanword.lua
    // 时返回空词表（禁词过滤不生效），不算加载错误。
    bool get_ban_words(std::vector<std::string>& out);

    // 专用：获取 emote 表情表（内部调用 LoadDataScript("emote")）
    // 结构：verb -> { field -> 文案 }；调用方遍历后填充 CMapping
    bool get_emote_map(
        std::map<std::string, std::map<std::string, std::string>>& out);

    // 专用：获取公告列表（内部调用 LoadDataScript("bullet") 解析
    // bullet_lines）
    bool get_bullet_lines(std::vector<std::string>& out);

    // 内部：加载 DATA 类型脚本（供 GetDataArray/GetDataMap 调用）
    int load_data_script_internal(const char* name);

    // 获取 lua_State（供绑定层使用）
    lua_State* get_state() { return m_state; }

    // 脚本根目录（用户目录）
    const char* get_script_root() { return m_script_root.c_str(); }
    // 默认脚本根目录（服务器自带）
    const char* get_default_script_root()
    {
        return m_default_script_root.c_str();
    }

    //---------------------------------------------------------
    // Lua call_out 回调机制
    //---------------------------------------------------------

    // 注册一个 Lua 延迟回调（owner 是发起 call_out 的对象，回调归属它）
    // 按名注册：函数必须已存在于 scriptRef 指向的脚本表中，否则报错并拒绝注册
    // 返回回调 ID（作为 call_out 的 param1 传递），-1 表示失败
    int register_callout(CContainer* owner, int scriptRef, const char* funcName,
                         int32_t userParam1 = 0, int32_t userParam2 = 0);

    // 按函数值注册：funcRef 是 LUA_REGISTRYINDEX
    // 中的函数引用（所有权移交注册表， 条目释放时
    // unref）。用于模块定义的回调挂到对象上的场景（如 yewai_grid）
    int register_callout_function(CContainer* owner, int scriptRef, int funcRef,
                                  int32_t userParam1 = 0,
                                  int32_t userParam2 = 0);

    // 执行一个 Lua 延迟回调（由 C++ 桥接函数调用）
    // ob 是回调对象，calloutId 是回调 ID
    void execute_callout(CContainer* ob, int32_t calloutId);

    // 当前正在执行的 Lua 脚本引用（供 call_out 在非 Lua 对象（如玩家）
    // 上注册回调时回退使用：取正在执行的脚本的 registry 引用）
    int get_active_script_ref() { return m_active_script_ref; }
    void set_active_script_ref(int ref) { m_active_script_ref = ref; }

    // 移除指定函数名的 call_out（用于 Lua 脚本的 remove_call_out）
    // 返回移除的数量
    int remove_callout_by_func_name(CContainer* ob, const char* funcName);

private:
    CLuaEngine();
    ~CLuaEngine();

    lua_State* m_state;
    std::string m_script_root;
    std::string m_default_script_root;  // 服务器自带默认脚本目录
    // 已加载脚本缓存：复合 key ("type_dir/bareName") -> 脚本状态
    // 复合 key 格式同 m_scriptMap，确保同名的 item/skill 脚本各自独立
    std::map<std::string, LuaScript> m_scripts;

    // 当前正在执行的脚本引用（LUA_NOREF 表示没有正在执行的 Lua 脚本）
    int m_active_script_ref;

    // Lua 回调注册表：回调 ID -> (脚本引用, Lua函数名或函数引用, 用户参数)
    struct LuaCallout {
        int scriptRef    = LUA_NOREF;
        int32_t owner_id = 0;  // 发起 call_out 的对象（object_id），remove
                               // 只删发起者自己的条目
        std::string funcName;  // 按名回调的函数名；函数值回调时为空
        int funcRef
            = LUA_NOREF;  // 函数值回调的 registry 引用，条目释放时 unref
        int32_t userParam1 = 0;
        int32_t userParam2 = 0;
    };
    std::map<int32_t, LuaCallout> m_callouts;
    int32_t m_next_callout_id;

    // 移除一个回调注册条目；函数值条目一并 unref 其 registry 引用
    void erase_callout(int32_t calloutId);

    // 当前正在执行的 callout ID（0 表示空闲），防止 Lua 回调中 remove_call_out
    // 删除自身导致迭代器失效
    int32_t m_executing_callout_id;

    // callout 日志（通过 Logger.h 分级输出到 log/MM-DD.txt）
    void log_callout(LogLevel level, const char* fmt, ...);

    // 根据类型获取脚本子目录
    std::string get_script_dir(LuaScriptType type);
    // 拼接完整路径
    std::string get_script_path(const char* name, LuaScriptType type);

    // 脚本加载失败时打印调用来源：Lua 调用栈（脚本+行号+函数）
    // 与 C++ 栈帧符号。仅异常路径调用。
    void dump_caller_info(const char* what);

    // 记录脚本加载失败（负缓存条目，ref=LUA_NOREF）：同一失败重复请求
    // 只回一行短错误；文件内容变化（含从无到有）后 load_script 重试首载
    void record_script_load_failure(const std::string& compositeKey,
                                    const std::string& path, LuaScriptType type,
                                    unsigned long contentHash,
                                    const char* errorMsg);

    // 房间类装配：脚本返回表声明 type = "resident" 时，自动把
    // room/resident_base.lua 混入该表（install 语义：脚本自定义同名钩子
    // 优先，并设置 resident_area/can_save）。地图脚本因此无需逐文件
    // require + install。要求栈顶为脚本返回表，返回后栈保持不变；
    // 装配失败只记日志，不阻断脚本加载。
    void apply_room_class_mixins();

    // 扫描脚本目录树，建立 复合key("type_dir/name") -> 完整路径 映射
    void scan_script_tree();
    // 递归扫描子目录（fromDefault 标记是否来自默认目录）
    void scan_dir(const char* dir, LuaScriptType type,
                  bool fromDefault = false);
    // 脚本文件映射：复合 key ("type_dir/bareName") -> 完整路径
    // 复合 key 格式：GetScriptDir(type) + "/" + 文件名（不含 .lua）
    // 例："item/hammer"、"skill/hammer"、"npc/pub1"
    std::map<std::string, std::string> m_script_map;
    // 脚本来源标记：复合 key -> 是否来自默认目录（不参与热重载）
    std::map<std::string, bool> m_script_default_flag;

    // 房间脚本索引：中文地名 -> 脚本名（拼音文件名，m_scripts 的 key）
    // 由 LoadRoomIndex() 扫描 lua/room/ 下的脚本并读取其 name 字段构建
    std::map<std::string, std::string> m_room_name_map;

    // 动态实例房间前缀索引：base_name 前缀 -> 共享脚本名
    // 由 load_room_index() 读取房间脚本的 prefix 字段构建
    std::map<std::string, std::string> m_room_prefix_map;

    // 动态房间名称列表（GBK 编码），由 LoadRoomIndex() 从房间脚本 name 字段构建

    std::vector<std::string> m_room_names;

public:
    // 获取房间数量（动态，替代 MAX_ROOM_NUM）
    int get_room_count() const { return (int)m_room_names.size(); }

    // 按索引获取房间名（GBK 编码，0-based）
    // 索引越界返回空字符串
    const char* get_room_name_by_index(int index) const;
};

// 供 --check-lua 语法校验模式使用的文件加载函数
// 读取 UTF-8 文件并转为 GBK（跳过 BOM）
std::string load_file_as_gbk(const char* path);
// Lua require 系统的 GBK 文件加载器（供 searcher 回调）
int gbk_file_loader(lua_State* L);

// 全局访问宏
#define g_LuaEngine CLuaEngine::instance()

// RAII 助手：在调用 Lua 脚本函数期间，记录"当前正在执行的脚本"引用。
// 脚本内 self:call_out(...) 时，若 self 不是 Lua 对象（如玩家），
// l_call_out 可回退到此引用，保证回调注册成功。
class CLuaActiveRefGuard {
public:
    CLuaActiveRefGuard(int scriptRef)
    {
        m_old = g_LuaEngine->get_active_script_ref();
        if (scriptRef != LUA_NOREF)
            g_LuaEngine->set_active_script_ref(scriptRef);
    }
    ~CLuaActiveRefGuard() { g_LuaEngine->set_active_script_ref(m_old); }

private:
    int m_old;
};

// RAII 助手：标记当前正在执行的 callout ID，防止 remove_call_out 重入删除。
// ExecuteCallout 进入时设置，退出时（包括异常路径）自动清除。
class CLuaCalloutExecGuard {
public:
    CLuaCalloutExecGuard(int32_t& target, int32_t id)
        : m_target(target), m_old(target)
    {
        target = id;
    }
    ~CLuaCalloutExecGuard() { m_target = m_old; }

private:
    int32_t& m_target;
    int32_t m_old;
};
