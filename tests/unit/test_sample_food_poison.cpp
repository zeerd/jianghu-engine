// test_sample_food_poison.cpp - sample 世界的食物与毒药两条链（脚本契约）
//
// 锁定三件事（样本脚本才是玩家真正吃到/用到的那一份，引擎契约绿了但样本
// 写错——比如不复用 food_base、毒药数值漂移、死亡不回链——玩家侧照样表现
// 为「吃不下 / 毒不掉血 / 不掉东西也不掉尸体」）：
//   1. item/sample_baozi.lua → lua-default/item/food_base.do_use：
//      残血吃下真回血、吃完销毁（finish_eat 缺省 0 = 销毁）；
//   2. item/sample_shedan.lua + condition/snake_poison.lua：
//      服下即时双损伤（hp 扣 40% 上限、eff_hp 扣 10% 上限）并挂 2 拍蛇毒；
//   3. npc/sample_qingshe.lua 的 die：蛇胆丢在脚下让玩家自己捡，且必须回链
//      die_base——不回链就不掉尸体、不结算击杀经验。
//
// 样本脚本经 ScopedSampleScript 拷进单测脚本根（拷原文，不与样本漂移）；
// 钩子表由 ensure_lua_env 每次重建，用例不必手工还原。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <vector>

#include <windows.h>  // LuaItem.inl 需 UUID（rpc 链经 windows.h）

#include "Char.h"
#include "Container.h"
#include "Corpse.h"
#include "Global.h"     // g_last_destruct_object
#include "HeartBeat.h"  // g_destruct_list 延迟销毁队列
#include "Item.h"
#include "ItemMgr.h"
#include "Npc.h"
#include "NpcMgr.h"
#include "Room.h"
#include "lua/LuaItem.h"
#include "mock/mock_itemmgr.h"
#include "test_log_hits.h"
#include "test_lua_env.h"
#include "test_sample_script.h"

namespace {

// 工厂是无参函数指针：把「本用例负责回收」的清单挂到文件级指针上
std::vector<CContainer*>* g_owned_items = nullptr;

// 按样本脚本造物件——hooks 与脚本里的 load_item 在单测里由 mock 管理器承接。
// 尸体是引擎造的（CChar::die → make_corpse 按基名装载），必须给回真实类型，
// 否则「标准死亡落地了什么」这条断言测不到真尸体。
CContainer* SampleItemFactory(const char* base_name)
{
    CContainer* ob = nullptr;
    if (std::strcmp(base_name, "corpse_ob") == 0) {
        ob = new CCorpse();
        ob->set("base_name", base_name);
        ob->create();
    }
    else {
        auto* it = new CLuaItem(base_name);
        it->set_name(base_name, base_name);
        it->create(0);
        ob = it;
    }
    if (g_owned_items) g_owned_items->push_back(ob);
    return ob;
}

class SampleFoodPoisonTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);

        // 样本脚本进单测脚本根；food_base 在 lua-default，
        // 作为 default_script_root 天然可见，无需搬运
        baozi_.reset(new ScopedSampleScript(
            "item/sample_baozi.lua", "lua-sample/item/sample_baozi.lua"));
        shedan_.reset(new ScopedSampleScript(
            "item/sample_shedan.lua", "lua-sample/item/sample_shedan.lua"));
        venom_.reset(
            new ScopedSampleScript("condition/snake_poison.lua",
                                   "lua-sample/condition/snake_poison.lua"));
        snake_.reset(new ScopedSampleScript(
            "npc/sample_qingshe.lua", "lua-sample/npc/sample_qingshe.lua"));
        ASSERT_TRUE(baozi_->ok()) << "样本脚本未就位: " << baozi_->file();
        ASSERT_TRUE(shedan_->ok()) << "样本脚本未就位: " << shedan_->file();
        ASSERT_TRUE(venom_->ok()) << "样本脚本未就位: " << venom_->file();
        ASSERT_TRUE(snake_->ok()) << "样本脚本未就位: " << snake_->file();

        saved_mgr        = g_item_mgr;
        g_item_mgr       = &mgr;
        mgr.load_factory = &SampleItemFactory;

        owned_.clear();
        g_owned_items = &owned_;

        room_.set("name", "sample_food_poison_room");
        room_.set("infinity", 1);

        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        queued_at_setup_.assign(g_destruct_list.begin(), g_destruct_list.end());
    }

    void TearDown() override
    {
        // 本用例销毁的对象停在延迟销毁队列里：出队，免得后续套件冲刷时
        // 对已回收的指针二次 delete
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                if (std::find(queued_at_setup_.begin(), queued_at_setup_.end(),
                              *it)
                    == queued_at_setup_.end()) {
                    it = g_destruct_list.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
        g_owned_items = nullptr;
        for (CContainer* ob : owned_) delete ob;
        owned_.clear();

        mgr.load_factory = nullptr;
        g_item_mgr       = saved_mgr;  // 恢复进程原值，不得置空
    }

    // 按样本脚本造物件（堆上，本用例负责回收）
    CLuaItem* MakeItem(const char* script)
    {
        auto* it = new CLuaItem(script);
        it->set_name(script, script);
        it->create(0);
        owned_.push_back(it);
        return it;
    }

    // 直接调 poison_base 的成员（模块级断言，不经样例物品自己的路由）——
    // 栈恒复位，调用失败记 ADD_FAILURE 并返回 -1
    int CallPoisonBase(const char* fn, CContainer* self, const char* comm,
                       CChar* me)
    {
        lua_State* L   = CLuaEngine::instance()->get_state();
        const int base = lua_gettop(L);

        lua_getglobal(L, "require");
        lua_pushstring(L, "item.poison_base");
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            ADD_FAILURE() << "require item.poison_base: "
                          << lua_tostring(L, -1);
            lua_settop(L, base);
            return -1;
        }
        lua_getfield(L, -1, fn);
        if (!lua_isfunction(L, -1)) {
            ADD_FAILURE() << "poison_base 没有 " << fn;
            lua_settop(L, base);
            return -1;
        }
        lua_bind_push_object(L, self);
        lua_pushstring(L, comm);
        lua_bind_push_object(L, me);
        lua_pushstring(L, "");
        if (lua_pcall(L, 4, 1, 0) != LUA_OK) {
            ADD_FAILURE() << "poison_base." << fn << ": "
                          << lua_tostring(L, -1);
            lua_settop(L, base);
            return -1;
        }
        const int ret = (int)lua_tointeger(L, -1);
        lua_settop(L, base);
        return ret;
    }

    // 地上的尸体：构造时注册了心跳，注销后再回收（工厂造的物件在 owned_ 里，
    // 这里取走所有权就要先出队，否则 TearDown 会二次 delete）
    bool TakeCorpseFromRoom()
    {
        CCorpse* corpse = nullptr;
        DTItemList items(*room_.get_item_list());
        for (CContainer* ob : items) {
            if (auto* c = dynamic_cast<CCorpse*>(ob)) {
                corpse = c;
                break;
            }
        }
        if (!corpse) return false;

        corpse->set_heart_beat(0);
        owned_.erase(std::remove(owned_.begin(), owned_.end(), corpse),
                     owned_.end());
        delete corpse;
        return true;
    }

    CMockItemMgr mgr;
    IItemMgr* saved_mgr = nullptr;
    CRoom room_;
    std::unique_ptr<ScopedSampleScript> baozi_;
    std::unique_ptr<ScopedSampleScript> shedan_;
    std::unique_ptr<ScopedSampleScript> venom_;
    std::unique_ptr<ScopedSampleScript> snake_;
    std::vector<CContainer*> owned_;
    std::vector<CContainer*> queued_at_setup_;
};

// ---------------------------------------------------------------------
// 食物：包子走 food_base 的通用吃食逻辑
// ---------------------------------------------------------------------
TEST_F(SampleFoodPoisonTest, Baozi_EatRestoresHpAndDestroysItem)
{
    CLuaItem* baozi = MakeItem("sample_baozi");

    EXPECT_EQ(1, baozi->query_attr(CMainObject::IS_FOOD)) << "type = \"food\"";
    EXPECT_EQ(1, baozi->can_use()) << "Can_Use 由 food_base 提供";
    EXPECT_EQ(1, baozi->query("food_remaining"));
    EXPECT_EQ(15, baozi->query("food_supply"));
    EXPECT_EQ(200, baozi->query("value"));

    CChar me;
    me.set_name("食客", "bz_eater");
    me.set("max_hp", (long)405);
    me.set("eff_hp", (long)405);
    me.set("hp", (long)300);
    me.set("max_mp", (long)200);
    me.set("mp", (long)100);

    const int32_t item_id = baozi->object_id();
    EXPECT_EQ(1, baozi->do_use(&me, nullptr)) << "food_base.do_use 成功返回 1";

    EXPECT_EQ(305, me.query("hp")) << "单口回血 = floor(food_supply / 3) = 5";
    EXPECT_EQ(100, me.query("mp")) << "没有 mp_supply 就不动内力";
    EXPECT_EQ(0, baozi->query("food_remaining")) << "一口吃完";
    EXPECT_EQ(item_id, g_last_destruct_object)
        << "finish_eat 缺省 0 → 销毁物品";
}

// ---------------------------------------------------------------------
// 毒药：蛇胆服下即双损伤，再挂 2 拍蛇毒
// ---------------------------------------------------------------------
TEST_F(SampleFoodPoisonTest,
       Shedan_SwallowDealsDoubleDamageAndMountsSnakePoison)
{
    CLuaItem* shedan = MakeItem("sample_shedan");

    EXPECT_EQ(1, shedan->can_use());
    EXPECT_EQ(2, shedan->query("poison_supply")) << "每颗贡献 2 拍蛇毒";
    EXPECT_EQ(3, shedan->query("index"))
        << "涂毒可用次数（poison_base.do_mo 消费）";
    EXPECT_STREQ("snake_poison", shedan->querystr("poison_type"))
        << "do_mo 挂状态、记 mo_type 都按它";
    EXPECT_EQ(0, shedan->query("value"));

    CChar me;
    me.set_name("试毒者", "sd_user");
    me.set("max_hp", (long)405);
    me.set("eff_hp", (long)405);
    me.set("hp", (long)405);

    const int32_t item_id = shedan->object_id();
    // 服下走 handle_action 的 use 分支——标准 use 命令之前它就被接住了
    EXPECT_EQ(1, shedan->handle_action((char*)"use", &me, (char*)""));

    EXPECT_EQ(243, me.query("hp")) << "服下即时扣 40% 气血上限（405×40/100）";
    EXPECT_EQ(365, me.query("eff_hp")) << "再扣 10% 上限（405×10/100）";
    EXPECT_EQ(2, me.query_condition("snake_poison"))
        << "按 poison_supply 挂 2 拍蛇毒——条件脚本装载成功才写得进";
    EXPECT_EQ(item_id, g_last_destruct_object) << "一颗一份，服完销毁";
}

// ---------------------------------------------------------------------
// 死亡掉落：青蛇的 die 把蛇胆丢在脚下，且必须回链标准死亡流程
// ---------------------------------------------------------------------
TEST_F(SampleFoodPoisonTest, Qingshe_DieDropsShedanAndRunsStandardDeath)
{
    CNpc* snake = load_npc("sample_qingshe");
    ASSERT_NE(snake, nullptr);
    snake->move(&room_);
    const int32_t snake_id = snake->object_id();

    snake->die();

    EXPECT_NE(room_.present("she dan"), nullptr)
        << "蛇胆掉在脚下，等玩家自己捡";
    EXPECT_EQ(snake_id, g_last_destruct_object)
        << "回链 die_base 才走标准死亡（自毁 + 击杀结算在此链上）";
    EXPECT_TRUE(TakeCorpseFromRoom()) << "尸体照常落地（腐烂链的前置）";

    g_npc_list.erase(snake_id);  // destruct 已摘除，这里兜脚本没回链的分支
    snake->set_heart_beat(0);
    snake->move(nullptr);
    delete snake;
}

// ---------------------------------------------------------------------
// 毒药面：poison_base 的观察 / 涂毒 / 路由（样例物品只挑它要的成员挂）
// ---------------------------------------------------------------------
TEST_F(SampleFoodPoisonTest, Shedan_LookShowsPoisonFace)
{
    CLuaItem* shedan = MakeItem("sample_shedan");

    CChar passer;
    passer.set_name("查看者", "sd_look");
    EXPECT_EQ(1, shedan->do_look(&passer)) << "无毒术：只显次数，不弹菜单";

    CChar adept;
    adept.set_name("用毒者", "sd_adept");
    adept.set_skill("poison", 100);
    shedan->move(&adept);  // 持有者才给涂毒菜单（environment == me）
    EXPECT_EQ(1, shedan->do_look(&adept)) << "持有 + 毒术 → 涂毒菜单分支";
    shedan->move(nullptr);
}

TEST_F(SampleFoodPoisonTest, Shedan_TumoWithPoisonSkillCoatsWeapon)
{
    CLuaItem* shedan = MakeItem("sample_shedan");

    CItem weapon;
    weapon.set_name("铁剑", "sfp_sword");
    weapon.set("skill_type", "sword");

    CChar me;
    me.set_name("用毒者", "sd_mo");
    me.set("max_hp", (long)405);
    me.set("hp", (long)405);
    me.set("mp", (long)100);
    me.set_skill("poison", 100);
    weapon.move(&me);
    (*me.get_equip_list())[WIELD_RIGHT_HAND] = weapon.object_id();

    const int err_before = count_log_hits("call_out: func 'remove_effect'");
    EXPECT_EQ(1, shedan->handle_action((char*)"tumo", &me, (char*)""))
        << "毒术 + 武器 + 内力齐备 → poison_base.do_mo 成功";
    EXPECT_EQ(err_before, count_log_hits("call_out: func 'remove_effect'"))
        << "remove_effect 必须挂在物品脚本表里，涂毒时效才登记得上";

    EXPECT_EQ(1, me.query_temp("poison/modu")) << "涂毒标记挂上";
    EXPECT_STREQ("snake_poison", me.querystr_temp("poison/mo_type"));
    EXPECT_EQ(70, me.query("mp")) << "涂毒耗 30 内力";
    EXPECT_EQ(2, shedan->query("index")) << "涂一次耗一剂";
    EXPECT_EQ(0, me.query_condition("snake_poison")) << "涂毒不等于服毒";

    weapon.move(nullptr);  // 局部对象不得留在 me 容器里被二次析构
}

TEST_F(SampleFoodPoisonTest, Shedan_TumoWithoutPoisonSkillSelfAppliesCondition)
{
    CLuaItem* shedan = MakeItem("sample_shedan");

    CChar me;
    me.set_name("莽夫", "sd_noskill");
    me.set("max_hp", (long)405);
    me.set("hp", (long)405);

    const int32_t item_id = shedan->object_id();
    EXPECT_EQ(0, shedan->handle_action((char*)"tumo", &me, (char*)""))
        << "没有毒术 → notify_fail 拒绝（返回 0）";
    EXPECT_EQ(3, me.query_condition("snake_poison"))
        << "毒按整剂 index 挂到自己身上（冒失涂抹分支）";
    EXPECT_EQ(item_id, g_last_destruct_object) << "一剂用完即销毁";
}

// 模块级：poison_base.handle_action 自己的三个分支（不经样例物品的路由）
TEST_F(SampleFoodPoisonTest, PoisonBase_HandleActionRoutesUseTumoAndUnknown)
{
    CLuaItem* by_use = MakeItem("sample_shedan");
    CChar user;
    user.set_name("服毒者", "pb_user");
    user.set("max_hp", (long)405);
    user.set("eff_hp", (long)405);
    user.set("hp", (long)405);

    EXPECT_EQ(1, CallPoisonBase("handle_action", by_use, "use", &user))
        << "use 分支：挂状态 + 销毁本剂";
    EXPECT_EQ(3, user.query_condition("snake_poison"))
        << "poison_base 的 use 按 index 挂时长（样例物品自己的 use 才用 "
           "poison_supply）";
    EXPECT_EQ(by_use->object_id(), g_last_destruct_object);

    // tumo 分支选「无毒术」：do_mo 走自毒支，不登记 call_out，
    // 模块直调（无活动脚本引用）也不会踩到回调注册
    CLuaItem* by_tumo = MakeItem("sample_shedan");
    CChar no_skill;
    no_skill.set_name("莽夫", "pb_noskill");
    no_skill.set("max_hp", (long)405);
    no_skill.set("hp", (long)405);

    EXPECT_EQ(0, CallPoisonBase("handle_action", by_tumo, "tumo", &no_skill))
        << "tumo 分支：无毒术 → do_mo 的拒绝支（notify_fail 返回 0）";
    EXPECT_EQ(3, no_skill.query_condition("snake_poison"));
    EXPECT_EQ(by_tumo->object_id(), g_last_destruct_object);

    // 未知命令：两个分支都不接管，交回引擎
    CLuaItem* quiet = MakeItem("sample_shedan");
    EXPECT_EQ(0, CallPoisonBase("handle_action", quiet, "bogus", &no_skill))
        << "未知命令返回 0，不吞命令";
    EXPECT_NE(quiet->object_id(), g_last_destruct_object) << "没被销毁";
}

}  // namespace
