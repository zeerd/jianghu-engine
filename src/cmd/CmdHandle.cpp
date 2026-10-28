/**
 * @file CmdHandle.cpp
 * @brief implementation of the CCmdHandle class. */

#include <windows.h>

#include "Global.h"
#include "CmdHandle.h"
#include "Channel.h"
#include "Skill.h"
#include "Room.h"
#include "CObject.h"
#include "../interfaces/IActionHandler.h"

#include "Sql.h"
#include "Combatd.h"
#include "CombatConfig.h"
#include "CombatRules.h"
#include "FightRoom.h"
#include "Vendor.h"
#include "Money.h"
#include "LuaHook.h"
#include "ServerConfig.h"

#include "CmdStandard.h"
#include "CmdUser.h"
#include "CmdWizard.h"

CCmdStandard g_cmd_standard;
CCmdUser g_cmd_user;
CCmdWizard g_cmd_wizard;

CCmdHandle::CCmdHandle() {}

CCmdHandle::~CCmdHandle() {}

extern int g_start_pk;

char g_notify_fail_msg[512];

int notify_fail(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_notify_fail_msg, sizeof(g_notify_fail_msg), fmt, ap);
    va_end(ap);
    return 0;
}

// 检查AP点数是否够
int check_ap(int ap, CChar* me)
{
    if (!me->is_fighting()) return 1;

    if (me->query_temp("fight/ap") < ap) {
        return notify_fail("需要%d点AP，你的AP不足。", ap);
    }
    return 1;
}

void resume_ap(int ap, CChar* me)
{
    if (!me->query_temp("fight/is_fighting")) return;

    me->add_temp("fight/ap", -ap);

    me->send_state();
}

// 判断两人是否亲属关系。
int is_relative(CChar* me, CChar* who)
{
    // 可以查看师傅的技能
    if (me->is_apprentice_of(who)) return 1;

    // 可以查看伴侣技能
    if (EQUALSTR(me->querystr("couple/id"), who->querystr("id"))
        && EQUALSTR(who->querystr("couple/id"), me->querystr("id")))
        return 1;

    // 可以看环境的信息
    if (me->environment() == who) return 1;

    if (who->environment() == me) return 1;

    return 0;
}

CChar* get_room_char(CChar* me, int32_t target)
{
    if (!target) return 0;
    CContainer* ob = (me->environment())->present(target);
    if (ob && ob->query_attr(CChar::IS_CHAR)) return static_cast<CChar*>(ob);
    return 0;
}

CContainer* get_room_object(CChar* me, int32_t target)
{
    if (!target) return 0;
    return (me->environment())->present(target);
}

CContainer* get_self_object(CChar* me, int32_t target)
{
    if (!target) return 0;
    return me->present(target);
}

// 丢弃身上一切非autoload的物品
void drop_all(CChar* me)
{
    DTItemList search;
    char cmd[255];
    CContainer *item, *env;
    CMapping* m;
    int count = 0;

    if ((env = me->environment())) {
        // 判断房间里是否还能容纳
        DTItemList* list = env->get_item_list();
        count            = env->query("max_items") - list->size();
    }

    copy_list(&search, me->get_item_list());

    for (auto val : search) {
        item = val;
        if (item->query_autoload()) continue;
        if (item->query_attr(CChar::IS_MONEY)) continue;  // 死亡后钱不损失

        m = item->query_entire_dbase();

        if (item->query("equipped")) {
            me->command(snprintf(cmd, 255, "unwield %ld", item->object_id()));
            if (item->query("index") && item->query("level"))
                item->set("owner", me->querystr("id"));
        }

        if (count-- <= 0 || !m->undefinedp("no_drop")) {
            item->move(load_room("room_void"));
        }
        else {
            item->move(env);
            message_vision(
                snprintf(cmd, 255, "$N丢下数量%ld的$n。", item->query_amount()),
                me, item);
        }
    }
}

// 处理命令：命令形式：命令，目标，行为细节
int CCmdHandle::command(char* Command, CChar* me)
{
    char Tar[512], arg[512];
    char Comm[512];
    int32_t target;
    CContainer* where;

    strncpy(Comm, Command, 512)[511] = 0;
    analyse_string(Comm, Tar, 512);
    analyse_string(Tar, arg, 512);

    if (!me) return 0;
    if (!(where = me->environment())) return 0;  // 无环境角色不可以发命令

    bool bDebug = g_combat_config.debug();
    int filter  = 0;

    if (strcmp(Comm, "downloadobject") == 0
        || strcmp(Comm, "downloadskill") == 0
        || strcmp(Comm, "downloaduser") == 0 || strcmp(Comm, "loc") == 0
        || strcmp(Comm, "mv") == 0)
        filter = 1;

    else if (filter)
        bDebug = false;
    else if (!is_living(me))
        return 0;
    else if (me->is_loading())
        return 0;

    if (!filter) g_notify_fail_msg[0] = 0;

    // 检查是否使用网路加速（配置可关闭限流，E2E 集成测试置 rate_limit=0）
    if (!filter && is_user(me) && CServerConfig::rate_limit()
        && me->add_temp("cmd/cnt", 1) > 15) {
        me->delete_temp("cmd/cnt");

        if (g_current_time - me->query_temp("cmd/time")
            < 5)  // 5秒内发超过10条指令。
        {
            int warning = me->add_temp("cmd/warning", 1);

            if (warning < 4) {
                tell_object(
                    me, snprintf(arg, 255,
                                 "$HIR对不起！您发送的指令过快！第%s次警告！",
                                 chinese_number(warning, Tar)));
            }
            else {
                me->disable_player("神志不清");
                message_vision("\n$N突然间手脚一阵抽搐，神志混乱不清。\n", me);

                if (me->is_fighting())
                    me->call_out(do_revive, 10);
                else
                    me->call_out(do_revive, 180);
            }

            me->set_temp("cmd/time", g_current_time);
            return 0;
        }
        else {
            me->set_temp("cmd/time", g_current_time);
        }
    }

    // 找出目标
    target = atol(Tar);

    // 替换其中的&符号
    replace_string(arg, "&", "", 512);

    // 环境所附加的动作, 可截获玩家命令。
    if (!filter
        && dynamic_cast<IActionHandler*>(where)->handle_action(Comm, me, arg,
                                                               Tar))
        return 1;

    // 物件截获玩家命令
    CContainer* who = NULL;
    if (!filter && target) {
        who = where->present(target);
        if (!who) who = me->present(target);

        // 跟踪所有指令
        if (who && bDebug)
            LOG_TRACE("来自%s(%s)：%s 对象%s", me->name(1), me->id(1), Command,
                      who->name(1));

        if (who) {
            if (who->is_character()) {
                if (is_living(who)) {
                    // 命令准入：数据集规则否决即吞掉本次命令并向房间播报文案
                    std::string gate = combat_rule_reason(
                        "command_gate", me, static_cast<CChar*>(who), Comm);
                    if (!gate.empty()) {
                        message_vision(gate.c_str(), me, who);
                        return 1;
                    }

                    if (dynamic_cast<IActionHandler*>(who)->handle_action(
                            Comm, me, arg))
                        return 1;
                }
            }
            else if (dynamic_cast<IActionHandler*>(who)->handle_action(Comm, me,
                                                                       arg))
                return 1;
        }
    }

    if (!who)  // 查找在线玩家
    {
        who = find_player(target);
        if (bDebug)
            LOG_TRACE("来自%s(%s)：%s", me->name(1), me->id(1), Command);
    }

    // 数字目标解析结果留档：who 下面会被 emote 判定清零，而未知命令
    // 日志要靠它说清 target=[11] 到底指哪个对象
    const std::string target_name = who ? who->name(1) : "";

    // 判断是否emote
    if (who && !who->is_character()) who = 0;

    // 「party」一词身兼两职：组队指令 CCmdUser::party 与门派频道动词，
    // 且下面 do_channel 先于 handlecommand 执行。区分点是消息体：
    //   组队邀请靠 /t 目标、无消息体（arg 为空）→ 必须跳过频道交给 handlecommand；
    //   门派发言带消息体（arg 非空）→ 走 do_channel 频道广播。
    // 若不区分，do_channel 对空消息静默 return 1（ChatChannel do_channel 第 5 步）
    // 会吞掉组队指令，CCmdUser::party 永不执行（原库组队名 pary、频道名 party
    // 本不冲突，jianghu 把组队改名为 party 才撞车）。
    const bool is_party_team_cmd =
        (strcmp(Comm, "party") == 0 && arg[0] == '\0');
    if (!is_party_team_cmd
        && g_channel.do_channel(me, static_cast<CChar*>(who), Comm, arg))
        return 1;

    if (handlecommand(Comm, me, target, arg)) return 1;

    // 搜索表情emote
    if (g_channel.do_emote(me, static_cast<CChar*>(who), Comm, 0)) return 1;

    // 指令执行失败信息返回
    char msg[512];
    if (strlen(g_notify_fail_msg)) {
        me->send_command(snprintf(msg, 512, "&C=%d&F=%s&M=%s&", NOTIFY_FAIL,
                                  Comm, g_notify_fail_msg),
                         SEND_WAIT_IMMEDIATE);
    }
    else {
        // 调试用：把发送者（名字(id)@房间）与原始整行（命令词 + 目标 +
        // 参数）落日志，便于排查拼写与参数问题；数字目标能解析出对象时
        // 附上对象名，否则纯数字 ID 脱离当时场景无从对照
        std::string tgt = Tar;
        if (!target_name.empty()) tgt += "(" + target_name + ")";
        LOG_WARN(
            "未知的命令 来自%s(%s)@%s raw=[%s] cmd=[%s] target=[%s] arg=[%s]",
            me->name(1), me->id(1), where->name(1), Command, Comm, tgt.c_str(),
            arg);
        tell_object(me, "未知的命令。");
    }

    return 0;
}

int CCmdHandle::handlecommand(char* cmd, CChar* me, int32_t target, char* arg)
{
    int ret;

    // 搜索标准命令
    if ((ret = g_cmd_standard.handlecommand(cmd, me, target, arg)) != -1)
        return ret;

    if (me->query("grant") >= CChar::GRANT_PLAYER) {
        // 搜索用户命令
        if ((ret = g_cmd_user.handlecommand(cmd, me, target, arg)) != -1)
            return ret;

        // 搜索巫师指令
        if (is_wizard(me)
            && (ret = g_cmd_wizard.handlecommand(cmd, me, target, arg)) != -1)
            return ret;
    }

    // Lua 层命令：C++ 三表未命中后分发，权限由脚本自检
    if ((ret = lua_call_command(cmd, me, target, arg)) != -1) return ret;

    return 0;
}

void CCmdHandle::do_revive(CContainer* ob, int32_t param1, int32_t param2)
{
    CChar* me = static_cast<CChar*>(ob);
    me->enable_player();
    message_vision("\n$N渐渐恢复了神志。\n", me);
}

void CCmdHandle::do_kick(CContainer* ob, int32_t param1, int32_t param2)
{
    CUser* me = static_cast<CUser*>(ob);

    // 战斗中不掉线.
    if (me->is_fighting()) {
        me->call_out(do_revive, 60);
        return;
    }

    drop_all(me);
    me->save_to_server();
    LOG_INFO("战斗超时踢出: %s(id:%s)", me->name(1), me->id(1));
    me->offline();
    DESTRUCT_OB(me, "cmd::kick");
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// 全局：drop_all(me) - 丢弃玩家全部物品（收费计费用）
int l_drop_all(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) {
        lua_pushboolean(L, 0);
        return 1;
    }
    CChar* me = dynamic_cast<CChar*>(obj);
    if (me) {
        drop_all(me);
        lua_pushboolean(L, 1);
    }
    else
        lua_pushboolean(L, 0);
    return 1;
}

// notify_fail(msg[, args...]) -> 0 (用于脚本返回失败)
// 支持 string.format 风格的可变参数（与 C++ 变参 notify_fail 对齐）
int l_notify_fail(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    int n           = lua_gettop(L);
    if (n > 1) {
        // string.format(fmt, ...)
        lua_getglobal(L, "string");
        lua_getfield(L, -1, "format");
        lua_pushvalue(L, 1);
        for (int i = 2; i <= n; i++) lua_pushvalue(L, i);
        lua_call(L, n, 1);
        const char* fmt = lua_tostring(L, -1);
        notify_fail("%s", fmt);
        lua_pop(L, 2);
    }
    else {
        notify_fail("%s", msg);
    }
    lua_pushinteger(L, 0);
    return 1;
}

// get_room_char(me, target) -> object|nil - 查找 me 所在房间内的角色
int l_get_room_char(lua_State* L)
{
    CChar* me = lua_check_char(L, 1);
    if (!me) {
        lua_pushnil(L);
        return 1;
    }
    int32_t target = (int32_t)luaL_checkinteger(L, 2);
    lua_bind_push_object(L, get_room_char(me, target));
    return 1;
}

// is_relative(me, who) -> number - 判断两人是否亲属/师徒/伴侣关系
int l_is_relative(lua_State* L)
{
    CChar* me  = lua_check_char(L, 1);
    CChar* who = lua_check_char(L, 2);
    lua_pushinteger(L, (me && who) ? is_relative(me, who) : 0);
    return 1;
}
