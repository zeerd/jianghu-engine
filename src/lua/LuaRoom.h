/**
 * @file LuaRoom.h
 * @brief Lua 脚本定义的地图/房间模板类，支持 CRoom/CResident/CFBRoom/CFB。
 */

//---------------------------------------------------------

#pragma once

#include "FB.h"
#include "FBRoom.h"
#include "Room.h"
#include "Resident.h"

#include <type_traits>
#include <vector>
#include "LuaInstanceRegistry.h"

// 编译期特性检测：T 是否含各可选基类方法
// （CFBRoom/CRoom/CResident 与 CFB 的虚函数集不同，需用标签分派
//   避免在不含该方法的基类 T 上实例化非法调用）

// 检测 T 是否含 create(int)（仅 CFB 系有：CItem/CFB 的 create(int nFlag)）
template <typename T, typename = void>
struct has_int_create : std::false_type {};

template <typename T>
struct has_int_create<T, decltype(std::declval<T&>().create(0), void())>
    : std::true_type {};

// 检测 T 是否含 valid_leave(CChar*, const char*)
// （CRoom/CResident/CFBRoom 有，CFB 无）
template <typename T, typename = void>
struct has_valid_leave : std::false_type {};

template <typename T>
struct has_valid_leave<T, decltype(std::declval<T&>().valid_leave(
                                       std::declval<CChar*>(),
                                       std::declval<const char*>()),
                                   void())> : std::true_type {};

// 检测 T 是否含 desc(CChar*)（仅 CFB 有，房间无）
template <typename T, typename = void>
struct has_desc : std::false_type {};

template <typename T>
struct has_desc<T, decltype(std::declval<T&>().desc(std::declval<CChar*>()),
                            void())> : std::true_type {};

template <typename T>
class CLuaRoomT : public T {
public:
    CLuaRoomT(const char* scriptName);
    virtual ~CLuaRoomT();

    // 重写 create()：调用 Lua 脚本的 create(self)（失败回退到 C++ 基类）
    // 适用于 CRoom/CResident/CFBRoom（无参 create）
    virtual void create();

    // 重写 create(int)：调用 Lua 脚本的 create(self, nFlag)
    // 仅 CFB 系基类存在该虚函数，故用标签分派处理基类回退
    virtual void create(int flag);

    // 重写 reset()：回退语义——脚本定义 reset 时完全接管，否则执行 C++ 基类
    // reset（NPC 复位/物品清理/房产税金）。内部经 reload_script() 判定。
    virtual void reset();

    // self:base_setup() / self:base_reset() - 供脚本显式调用 C++ 基类
    // setup/reset（脚本接管钩子后需要基类副作用时使用，非虚限定调用防递归）
    void base_setup() { T::setup(); }
    void base_reset() { T::reset(); }
    // self:base_handle_action4(comm, me, arg, tar) - 供脚本显式调用 C++ 基类
    // 4 参 handle_action（CRoom 的 back 返回总部等；非虚限定调用防递归）
    int base_handle_action4(char* comm, CChar* me, char* arg, char* tar)
    {
        return T::handle_action(comm, me, arg, tar);
    }
    // self:base_init(me) / self:base_valid_leave(me, dir) - 供脚本显式调用
    // C++ 基类 init / valid_leave（非虚限定调用防递归）
    void base_init(CChar* me) { T::init(me); }
    int base_valid_leave(CChar* me, const char* dir)
    {
        return base_valid_leave_impl(has_valid_leave<T>(), me, dir);
    }

    // 刷新脚本引用并调用 Lua reset(self) 回调（热重载用）
    // 返回 1 表示脚本定义了 reset 并已调用；0 表示未加载/未定义
    int reload_script();

    // 重写 init()：调用 Lua 脚本的 init(self, me)（若脚本定义了 init）
    virtual void init(CChar* me);

    // 重写 do_look()：调用 Lua 脚本的 do_look(self, me)（若脚本定义了 do_look）
    virtual int do_look(CChar* me);

    // 重写 setup()：调用 Lua 脚本的 setup(self)（若脚本定义了 setup）
    virtual void setup();

    // 重写 query_attr()：CAN_SAVE 从脚本 can_save 字段读取（动态实例房间
    // 的 load() 早于 create()，dbase 键来不及生效）；其余属性透传基类
    virtual int32_t query_attr(int attr);

    // 重写 handle_action()：调用 Lua 脚本的 handle_action(self, comm, me, arg,
    // tar)
    virtual int handle_action(char* comm, CChar* me, char* arg, char* tar);

    // 重写 valid_leave()：调用 Lua 脚本的 valid_leave(self, me, dir)
    // （CRoom/CResident/CFBRoom 有基类方法；CFB 无，用标签分派避免非法回退）
    virtual int valid_leave(CChar* me, const char* dir);

    // 重写 desc()：调用 Lua 脚本的 desc(self, me)（仅 CFB 有基类方法）
    virtual void desc(CChar* me);

    // 脚本名 / 脚本引用（供绑定层使用）
    const char* get_script_name() { return m_script_name.c_str(); }
    int get_script_ref() { return m_script_ref; }

private:
    std::string m_script_name;
    int m_script_ref;  // 脚本 table 的 registry 引用

    // 标签分派：仅当基类 T 确实含 create(int) 时才回退到 T::create(nFlag)
    void create_int_impl(std::true_type, int flag);
    void create_int_impl(std::false_type, int flag);

    // 标签分派：仅当基类 T 确实含 valid_leave 时才回退到 T::valid_leave
    int valid_leave_impl(std::true_type, CChar* me, const char* dir);
    int valid_leave_impl(std::false_type, CChar* me, const char* dir);

    // 标签分派：仅当基类 T 确实含 desc 时才回退到 T::desc
    void desc_impl(std::true_type, CChar* me);
    void desc_impl(std::false_type, CChar* me);
    // 标签分派：仅当基类 T 含 valid_leave 时非虚调用，否则默认放行
    int base_valid_leave_impl(std::true_type, CChar* me, const char* dir)
    {
        return T::valid_leave(me, dir);
    }
    int base_valid_leave_impl(std::false_type, CChar*, const char*)
    {
        return 1;
    }
};

// 类型别名
typedef CLuaRoomT<CRoom> CLuaRoom;
typedef CLuaRoomT<CResident> CLuaResident;
typedef CLuaRoomT<CFBRoom> CLuaFBRoom;
typedef CLuaRoomT<CFB> CLuaFB;

#include "Encoding.h"
#include "LuaRoom.inl"
