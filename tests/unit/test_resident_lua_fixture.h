// test_resident_lua_fixture.h — 居民区 Lua 化测试夹具
//
// 住宅区脚本 fixture_resident_room.lua 安装 room/resident_base.lua 的通用
// 逻辑；房屋由前缀动态房脚本 room/house_base.lua 承载（house_<属区>_<户主>
// 存档行由 MockDatabase 提供）。菜单与 say 的投递经 recording_char 捕获；
// 脚本函数经 call_script 直接调用；house_base 对属区的 house_price /
// count_days / update_register 为 Lua 直调 resident_base 模块函数。
//
// Build+Run: .\run-coverage.ps1
#pragma once

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <mutex>
#include <string>
#include <vector>

#include "Box.h"
#include "Char.h"
#include "GameInit.h"
#include "HeartBeat.h"
#include "Item.h"
#include "Login.h"
#include "LuaBind.h"
#include "LuaEngine.h"
#include "LuaRoom.h"
#include "Mapping.h"
#include "Money.h"
#include "Room.h"
#include "RoomMgr.h"
#include "User.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_npc_cleanup.h"
#include "test_online_user.h"

extern char g_notify_fail_msg[512];

namespace resident_lua_test {

// 固定维护时钟：用例内以 g_current_time 为基准构造"上次付费时间"
constexpr int k_now = 1700000000;

// 物品工厂：box→CBox，coin→CMCoin（已 create），其余 CItem。
// CMoney::player_pay/pay_player 经 g_item_mgr->load_item("coin") 取钱。
inline CContainer* item_factory(const char* base_name)
{
    CContainer* ob = nullptr;
    if (strcmp(base_name, "box") == 0) {
        ob = new CBox();
    }
    else if (strcmp(base_name, "coin") == 0) {
        CMCoin* coin = new CMCoin();
        coin->create();
        ob = coin;
    }
    else {
        ob = new CItem();
    }
    ob->set("base_name", base_name);
    return ob;
}

// 捕获投递内容的角色：菜单/say 均经 send_command，可断言文案
class recording_char : public CChar {
public:
    void send_command(const char* msg, int wait = SEND_WAIT_NONE) override
    {
        if (msg) msgs.emplace_back(msg);
    }

    bool has(const char* needle) const
    {
        for (const auto& m : msgs) {
            if (m.find(needle) != std::string::npos) return true;
        }
        return false;
    }

    void clear() { msgs.clear(); }

    std::vector<std::string> msgs;
};

// 失败诊断：把已投递文案拼进断言输出（便于判断走了哪个分支）
inline std::string dump_msgs(const recording_char& c)
{
    std::string s;
    for (const auto& m : c.msgs) s += m + " || ";
    return s;
}

// call_script 的参数：对象 / 字符串（原样字节，编码由调用方负责）/ 整数
struct lua_arg {
    enum class kind { object, text, number };

    kind k          = kind::number;
    CMainObject* ob = nullptr;
    const char* s   = nullptr;
    long n          = 0;
};

inline lua_arg object_arg(CMainObject* ob)
{
    lua_arg a;
    a.k  = lua_arg::kind::object;
    a.ob = ob;
    return a;
}

inline lua_arg text_arg(const char* s)
{
    lua_arg a;
    a.k = lua_arg::kind::text;
    a.s = s;
    return a;
}

inline lua_arg number_arg(long v)
{
    lua_arg a;
    a.k = lua_arg::kind::number;
    a.n = v;
    return a;
}

// 住宅区 + MockDatabase + 物品工厂的公共夹具
class fixture : public ::testing::Test {
protected:
    MockDatabase* db_    = nullptr;
    CMockItemMgr* mgr_   = nullptr;
    IItemMgr* saved_mgr_ = nullptr;
    CLuaRoom* resident_  = nullptr;
    int saved_time_      = 0;
    // 用例内造的物件（标记物/现金），测试体结束前销毁
    std::vector<CContainer*> items_;
    // SetUp 时延迟析构列表的快照：TearDown 只回收本用例新增的自毁对象
    std::vector<CContainer*> live_at_setup_;
    // 已删房体指针：防止 g_room_list 与 g_destruct_list 双登记导致双删
    std::vector<CContainer*> deleted_rooms_;
    // add_house_entries 的键序号：保证多次追加不覆盖
    int house_seq_ = 0;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        saved_time_    = g_current_time;
        g_current_time = k_now;
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            live_at_setup_.assign(g_destruct_list.begin(),
                                  g_destruct_list.end());
        }
        db_  = new MockDatabase();
        g_db = db_;
        // lock_data(1) 忽略失败直接返 1；lock_data(0) 查到已锁行才立即成功
        // ——不预置 obj_lock 则每次解锁忙等约 4 秒（见 test_house.cpp）
        db_->SetQueryResultMatching("select obj_lock",
                                    MockDatabase::MakeSingleRow({"1"}));
        resident_ = new CLuaRoom("fixture_resident_room");
        resident_->set("base_name", "resident_test_a");
        resident_->create();  // 脚本 create 写入区名/城名/场景描述
        resident_->setup();   // 脚本 setup：维护窗口 + 存档装载
        g_room_list["resident_test_a"] = resident_;
        saved_mgr_                     = g_item_mgr;
        mgr_                           = new CMockItemMgr();
        mgr_->load_factory             = &item_factory;
        g_item_mgr                     = mgr_;
    }

    void TearDown() override
    {
        // 1) 先摘仆人：此时房屋仍活着，且房内物件全活（move 遍历列表安全）
        discard_servants();
        // 2) 用例自造的物件（纯 delete，不碰悬垂宿主）
        free_items();
        // 3) load_room 造出的房屋：柜子先 destruct 进延迟列表，房体再删
        delete_houses();
        // 4) 延迟析构列表：只回收本用例新增、且未在上面删过的对象
        drain_destruct_list();
        // 5) 摘属区与 mgr/db，恢复时钟
        g_room_list.erase("resident_test_a");
        delete resident_;
        resident_  = nullptr;
        g_item_mgr = saved_mgr_;
        delete mgr_;
        mgr_ = nullptr;
        g_db = nullptr;
        delete db_;
        db_            = nullptr;
        g_current_time = saved_time_;
    }

    void discard_servants()
    {
        while (true) {
            CNpc* npc = find_npc_by_base_name("monster_yongren");
            if (!npc) break;
            discard_npc(npc);
        }
    }

    void free_items()
    {
        for (auto* it : items_) {
            delete it;
        }
        items_.clear();
    }

    void delete_houses()
    {
        std::vector<std::string> keys;
        for (auto& kv : g_room_list) {
            if (kv.first.rfind("house_", 0) == 0) keys.push_back(kv.first);
        }
        for (auto& key : keys) {
            CRoom* r = g_room_list[key];
            r->remove_all_box();
            g_room_list.erase(key);
            deleted_rooms_.push_back(r);
            delete r;
        }
    }

    bool live_at_setup(CContainer* ob) const
    {
        for (auto* p : live_at_setup_) {
            if (p == ob) return true;
        }
        return false;
    }

    bool deleted_here(CContainer* ob) const
    {
        for (auto* p : deleted_rooms_) {
            if (p == ob) return true;
        }
        return false;
    }

    void drain_destruct_list()
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        for (auto it = g_destruct_list.begin(); it != g_destruct_list.end();) {
            CContainer* ob = *it;
            if (live_at_setup(ob)) {  // 用例前已存在：原样保留
                ++it;
                continue;
            }
            it = g_destruct_list.erase(it);
            if (!deleted_here(ob)) delete ob;
        }
    }

    // 预置某房屋的存档行（obj_id 精确匹配）。行内不含 belong：
    // belong 为空时建房触发的 house_base.lua check_pays 直接返回，不干扰断言。
    // 必须带 base_name 与 infinity：load() 会整体替换 dbase（copy_mapping
    // 先清空）。生产存档序列化整个 dbase，天然含这两项——缺 base_name 会让
    // destruct 时 remove_room 摘不掉房间；缺 infinity 会让 get_local_house
    // 的二次 load 抹掉建房标记，人物 move 进房被重量检查拒绝。
    void preset_house_archive(const char* key, const char* owner, int32_t size)
    {
        CMapping archive;
        archive.set("base_name", (std::string("house_测试城_") + key).c_str());
        archive.set("name", "雅室");
        archive.set("owner", owner);
        archive.set("size", size);
        archive.set("infinity", 1);
        db_->SetQueryResultMatching(
            std::string("obj_id='house_测试城_") + key + "'",
            MockDatabase::MakeSingleRow({archive.get_buffer()}));
    }

    // 造一份住户登记（house/<户主> 子映射）。owner 字段语义与生产
    // update_register 一致：写的是第二房主（check_second_owner 据此配菜单）。
    void register_house_entry(const char* owner, int days_behind,
                              const char* name         = "雅室",
                              const char* short_name   = "房主",
                              const char* second_owner = "")
    {
        CMapping m;
        m.set("time", static_cast<long>(k_now - days_behind * 86400));
        m.set("name", name);
        m.set("short", short_name);
        m.set("owner", second_owner);
        char key[64];
        snprintf(key, sizeof(key), "house/%s", owner);
        resident_->set(key, m);
    }

    // 在属区登记 count 户（can_apply_house/house_price 的户数门槛）。
    // 键带全局序号：多次追加互不覆盖。
    void add_house_entries(int count)
    {
        char key[32];
        for (int i = 0; i < count; i++) {
            CMapping m;
            m.set("short", "住户");
            snprintf(key, sizeof(key), "house/h%04d", house_seq_++);
            resident_->set(key, m);
        }
    }

    CContainer* make_item(const char* base_name)
    {
        CItem* it = new CItem();
        it->set("base_name", base_name);
        it->set("id", base_name);
        items_.push_back(it);
        return it;
    }

    // 造一叠现金交到角色身上（支付/找零用例）；自毁后须 forget_coin
    CMCoin* make_coin(CChar* me, int32_t amount)
    {
        CMCoin* coin = new CMCoin();
        coin->create();
        coin->set("base_name", "coin");
        coin->set_amount(amount);
        coin->move(me);
        items_.push_back(coin);
        return coin;
    }

    // 现金被 set_amount(0) 自毁进延迟析构列表后，从用例物件表摘除
    void forget_coin_if_destroyed(CChar* me)
    {
        if (!me->present("coin")) items_.pop_back();
    }

    // 直接调用住宅区脚本函数（self 为属区房间），返回其整数结果
    long call_script(const char* name, std::initializer_list<lua_arg> args = {})
    {
        lua_State* L = g_LuaEngine->get_state();
        int ref      = resident_->get_script_ref();
        if (!L || ref == LUA_NOREF) return k_no_script;

        CLuaActiveRefGuard guard(ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        lua_getfield(L, -1, name);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            return k_no_script;
        }

        lua_bind_push_object(L, resident_);
        int nargs = 1;
        for (const auto& a : args) {
            switch (a.k) {
                case lua_arg::kind::object:
                    lua_bind_push_object(L, a.ob);
                    break;
                case lua_arg::kind::text:
                    lua_pushstring(L, a.s ? a.s : "");
                    break;
                case lua_arg::kind::number:
                    lua_pushinteger(L, (lua_Integer)a.n);
                    break;
            }
            nargs++;
        }

        if (lua_pcall(L, nargs, 1, 0) != LUA_OK) {
            ADD_FAILURE() << "Lua " << name << " 失败: " << lua_tostring(L, -1);
            lua_pop(L, 2);
            return k_lua_error;
        }

        long result = (long)lua_tointeger(L, -1);
        lua_pop(L, 2);  // 弹出结果和 table
        return result;
    }

    static constexpr long k_no_script = -1000;
    static constexpr long k_lua_error = -1001;
};

}  // namespace resident_lua_test
