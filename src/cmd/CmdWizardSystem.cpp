#include "CmdWizard.h"
#include "../skill/Skill.h"
#include "../engine/Channel.h"
#include "../room/Room.h"
#include "../item/FightRoom.h"
#include "../combat/CombatConfig.h"

int CCmdWizard::full(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CChar* who;

    if (!(who = get_room_char(me, target)) && !(who = find_player(target)))
        return 0;

    who->set("hp", who->query("max_hp"));
    who->set("eff_hp", who->query("max_hp"));
    who->set("mp", who->query("max_mp"));
    who->set("eff_mp", who->query("max_mp"));
    snprintf(msg, 255, "只见%s浑身金光乍现，散发无穷活力！", who->name());

    who->flush_my_info();
    message_vision_room(who, "chat", msg);
    return 1;
}

int CCmdWizard::summon(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CChar* who;

    if (!(who = find_player(target))) return notify_fail("你想抓谁过来？");

    // 判断权限
    if (who->query_grant() >= me->query_grant())
        return notify_fail("你的等级不够抓对方过来。");

    message_vision("$HIB突然一道闪电掠过，$N的身影在烟雾中消失了。", who);
    who->move(me->environment());
    tell_object(who, snprintf(msg, 255,
                              "你一阵头晕目眩，再睁眼时发现是%s把你弄过来的。",
                              me->name()));

    tell_room(
        me->environment(),
        snprintf(msg, 255, "$HIG一朵莲花冉冉盛开，你看到%s从莲心上跳了下来。",
                 who->name()),
        who);
    return 1;
}

// 瞬间移动
int CCmdWizard::goto_cmd(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CChar* who;
    CContainer* env;

    if (arg[0]) {
        if (!(env = load_room(arg))) return notify_fail("你想去哪？");
    }
    else {
        if (!(who = find_player(target))) return notify_fail("你想去哪里？");

        env = who->environment();
    }

    if (!env) return notify_fail("你去不了外太空！");

    // 客座巫师只能去jy
    if (me->query("grant") < 70 && !env->query("wiz_room"))
        return notify_fail("你还是走着去吧。");

    message_vision("$BLK只见$N伸手在空中画了个门，矮身钻了进去。", me);

    me->move(env);

    tell_room(env,
              snprintf(msg, 255, "$BLK空中飘过一朵祥云，%s从云上跳了下来。",
                       me->name()),
              me);

    return 1;
}

// 停止战斗
int CCmdWizard::stop_battle(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CContainer* env = me->environment();

    if (!env->query_attr(CChar::IS_FIGHTROOM)) return 0;
    snprintf(msg, 255, "%s大喝一声：反了反了，上班时间玩游戏！都给我干活去！",
             me->name());
    message_vision_room(me, "chat", msg);

    static_cast<CFightRoom*>(env)->end_fight();  // 结束战斗

    return 1;
}

// 日志级别名称表，下标与 LogLevel 枚举一致
static const char* const s_log_level_names[]
    = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

static const char* log_level_name(LogLevel level)
{
    if (level < LOG_LVL_TRACE || level > LOG_LVL_FATAL) return "???";
    return s_log_level_names[level];
}

// 严格解析级别名（大小写不敏感），失败返回 false 且不修改 out
static bool parse_log_level_name(const char* str, LogLevel& out)
{
    for (int i = LOG_LVL_TRACE; i <= LOG_LVL_FATAL; i++) {
        if (_stricmp(str, s_log_level_names[i]) == 0) {
            out = static_cast<LogLevel>(i);
            return true;
        }
    }
    return false;
}

// 设置系统参数

int CCmdWizard::sys_param(CChar* me, int32_t target, char* arg)
{
    char arg1[255], arg2[255];

    analyse_string(arg, arg1, 255);
    analyse_string(arg1, arg2, 255);

    if (strcmp(arg, "set") == 0 || strcmp(arg, "setstr") == 0) {
        // 保留键 log_level：运行时日志级别
        if (strcmp(arg1, "log_level") == 0) {
            LogLevel level = LOG_LVL_TRACE;
            if (arg2[0] == '-' || (arg2[0] >= '0' && arg2[0] <= '9')) {
                level = static_cast<LogLevel>(atoi(arg2));
                if (level < LOG_LVL_TRACE || level > LOG_LVL_FATAL)
                    return notify_fail("日志级别须为%d-%d或级别名。",
                                       LOG_LVL_TRACE, LOG_LVL_FATAL);
            }
            else if (!parse_log_level_name(arg2, level)) {
                return notify_fail("无效日志级别：%s。", arg2);
            }
            logger_set_level(level);

            snprintf(arg, 255, "%s将日志级别设置为%s (%d)。", me->name(1),
                     log_level_name(level), (int)level);
            tell_object(me, arg);
            g_channel.do_channel(&g_channel, 0, "sys", arg);
            LOG_INFO("日志级别被%s设置为%s", me->name(1),
                     log_level_name(level));
            return 1;
        }

        if (strcmp(arg, "set") == 0
            && (arg2[0] == '-' || (arg2[0] >= '0' && arg2[0] <= '9')))
            g_combat_config.set(arg1, atol(arg2));
        else
            g_combat_config.set(arg1, arg2);

        tell_object(me, snprintf(arg, 255, "%s设定系统参数%s为%s。",
                                 me->name(1), arg1, arg2));
        g_channel.do_channel(&g_channel, 0, "sys", arg);
        return 1;
    }

    if (strcmp(arg, "delete") == 0) {
        if (strcmp(arg1, "log_level") == 0)
            return notify_fail("log_level 不可删除。");
        g_combat_config.del(arg1);
        tell_object(me, snprintf(arg, 255, "删除系统参数%s。", arg1));

        return 1;
    }

    if (strcmp(arg, "query") == 0) {
        if (strcmp(arg1, "log_level") == 0) {
            LogLevel cur = logger_get_level();
            snprintf(arg, 255, "系统参数：%s=%s (%d)", arg1,
                     log_level_name(cur), (int)cur);
            tell_object(me, arg);
            return 1;
        }
        snprintf(arg, 255, "系统参数：%s=%d", arg1,
                 g_combat_config.query(arg1));
        tell_object(me, arg);
        return 1;
    }

    if (strcmp(arg, "querystr") == 0) {
        if (strcmp(arg1, "log_level") == 0) {
            snprintf(arg, 255, "系统参数：%s=%s", arg1,
                     log_level_name(logger_get_level()));
            tell_object(me, arg);
            return 1;
        }
        snprintf(arg, 255, "系统参数：%s=%s", arg1,
                 g_combat_config.querystr(arg1));
        tell_object(me, arg);
        return 1;
    }

    return 0;
}

// 设置技能参数 skillparam tianyu_qijian set valid_force bahuang_gong
int CCmdWizard::skill_param(CChar* me, int32_t target, char* arg)
{
    char arg0[255], arg1[255], arg2[255];

    analyse_string(arg, arg0, 255);
    analyse_string(arg0, arg1, 255);
    analyse_string(arg1, arg2, 255);

    CSkill* sk = load_skill(arg);
    strcpy(arg, sk->name(1));

    if (strcmp(arg0, "set") == 0 || strcmp(arg0, "setstr") == 0) {
        if (strcmp(arg0, "set") == 0
            && (arg2[0] == '-' || (arg2[0] >= '0' && arg2[0] <= '9')))
            sk->set(arg1, atol(arg2));
        else
            sk->set(arg1, arg2);

        tell_object(me, snprintf(arg0, 255, "%s设定技能%s参数%s为%s。",
                                 me->name(1), arg, arg1, arg2));
        g_channel.do_channel(&g_channel, 0, "sys", arg0);
        return 1;
    }

    if (strcmp(arg0, "delete") == 0) {
        sk->del(arg1);
        tell_object(me, snprintf(arg0, 255, "删除技能%s参数%s。", arg, arg1));

        return 1;
    }

    if (strcmp(arg0, "query") == 0) {
        snprintf(arg0, 255, "技能%s参数：%s=%ld", arg, arg1, sk->query(arg1));
        tell_object(me, arg0);
        return 1;
    }

    if (strcmp(arg0, "querystr") == 0) {
        snprintf(arg0, 255, "技能%s参数：%s=%s", arg, arg1, sk->querystr(arg1));
        tell_object(me, arg0);
        return 1;
    }

    return 0;
}

int CCmdWizard::call(CChar* me, int32_t target, char* arg)
{
    char arg1[255], arg2[255];
    CContainer* who;

    if (!target)
        who = me->environment();
    else if (!(who = get_room_object(me, target))
             && !(who = get_self_object(me, target))
             && !(who = find_player(target)))
        return notify_fail("请选中目标。");

    analyse_string(arg, arg1, 255);
    analyse_string(arg1, arg2, 255);

    if (strcmp(arg, "set") == 0 || strcmp(arg, "setstr") == 0) {
        if (strcmp(arg1, "grant") == 0) return notify_fail("请使用promote。");
        if (strcmp(arg1, "点数") == 0)
            return notify_fail("请使用增值服务支票。");

        if (strcmp(arg, "set") == 0
            && (arg2[0] == '-' || (arg2[0] >= '0' && arg2[0] <= '9')))
            who->set(arg1, atol(arg2));
        else {
            if (strcmp(arg1, "name") == 0)
                who->set_name(arg2);
            else if (strcmp(arg1, "id") == 0)
                who->set_name(NULL, arg2);
            else
                who->set(arg1, arg2);
        }

        who->update_me();

        tell_object(me, snprintf(arg, 255, "%s设定%s的%s为%s。", me->name(1),
                                 who->name(1), arg1, arg2));
        g_channel.do_channel(&g_channel, 0, "sys", arg);
        LOG_INFO("%s", arg);
        return 1;
    }

    if (strcmp(arg, "delete") == 0) {
        if (strcmp(arg1, "grant") == 0) return notify_fail("请使用promote。");
        if (strcmp(arg1, "点数") == 0)
            return notify_fail("请使用增值服务支票。");
        if (strcmp(arg1, "family") == 0 && is_user(who))
            static_cast<CChar*>(who)->release_master();

        who->del(arg1);
        who->update_me();
        tell_object(me, snprintf(arg, 255, "删除%s。", arg1));

        return 1;
    }

    if (strcmp(arg, "query") == 0) {
        snprintf(arg, 255, "%s=%ld", arg1, who->query(arg1));
        tell_object(me, arg);
        return 1;
    }

    if (strcmp(arg, "querystr") == 0) {
        snprintf(arg, 255, "%s=%s", arg1, who->querystr(arg1));
        tell_object(me, arg);
        return 1;
    }

    // 临时数据库
    if (strcmp(arg, "set_temp") == 0) {
        if (arg2[0] == '-' || (arg2[0] >= '0' && arg2[0] <= '9'))
            who->set_temp(arg1, atol(arg2));
        else
            who->set_temp(arg1, arg2);

        tell_object(me, snprintf(arg, 255, "设定临时%s为%s。", arg1, arg2));
        return 1;
    }

    if (strcmp(arg, "delete_temp") == 0) {
        who->delete_temp(arg1);
        who->update_me();
        tell_object(me, snprintf(arg, 255, "删除临时%s。", arg1));

        return 1;
    }

    if (strcmp(arg, "query_temp") == 0) {
        snprintf(arg, 255, "临时%s=%ld", arg1, who->query_temp(arg1));
        tell_object(me, arg);
        return 1;
    }

    if (strcmp(arg, "querystr_temp") == 0) {
        snprintf(arg, 255, "临时%s=%s", arg1, who->querystr_temp(arg1));
        tell_object(me, arg);
        return 1;
    }
    return 0;
}

// set_skill sword 20

int CCmdWizard::set_skill(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char level[40], msg[255];

    if (!(who = get_room_char(me, target)) && !(who = find_player(target)))
        return notify_fail("没有这个人。");

    analyse_string(arg, level, 40);
    if (who->set_skill(arg, atol(level))) {
        me->flush_my_skill(me);
        tell_object(me, snprintf(msg, 255, "%s设置%s的技能%s为%d级。",
                                 me->name(1), who->name(1), arg, atol(level)));
        g_channel.do_channel(&g_channel, 0, "sys", msg);
        LOG_INFO("%s", msg);
        return 1;
    }

    return notify_fail("没有这项技能。");
}

// 恢复技能经验1/3
int CCmdWizard::restore_exp(CChar* me, int32_t target, char* arg)
{
    CChar* who;

    if (!(who = find_player(target))) return notify_fail("没有这个人。");

    int32_t exp        = who->query("sk_exp");
    int32_t combat_exp = who->query_exp();

    if (exp * 3 < combat_exp)
        return notify_fail("此人经验%ld，技能经验%ld，不能恢复。", combat_exp,
                           exp);

    LOG_INFO("恢复%s(%s)的技能经验：%ld -> %ld", who->name(1),
             who->querystr("id"), exp, exp * 2 / 3);
    who->set("sk_exp", exp * 2 / 3);
    who->flush_my_info();
    tell_object(me, "OK!");
    return 1;
}

// 清掉某人所有技能。
int CCmdWizard::clear_skill(CChar* me, int32_t target, char* arg)
{
    CChar* who;

    if (!(who = find_player(target))) return notify_fail("没有这个人。");

    CMapping* skills = who->query_skills();
    skills->clear();

    skills = who->query_skill_prepare();
    skills->clear();

    skills = who->query_skill_map();
    skills->clear();

    skills = who->query_learned();
    skills->clear();

    who->del("sk_exp");
    who->send_state(false);

    tell_object(me, "OK!");
    return 1;
}

// 查看更改recruit_exp表
int CCmdWizard::recruit_exp(CChar* me, int32_t target, char* arg)
{
    char arg1[80], arg2[80], arg3[80];

    analyse_string(arg, arg1, 80);
    analyse_string(arg1, arg2, 80);
    analyse_string(arg2, arg3, 80);

    CMapping* ptr = g_channel.query_recruit(arg1);
    if (!ptr) return notify_fail("没有该玩家的收徒记录！");

    if (strcmp(arg, "set") == 0) {
        if (arg3[0] == '-' || (arg3[0] >= '0' && arg3[0] <= '9'))
            ptr->set(arg2, atol(arg3));
        else
            ptr->set(arg2, arg3);

        tell_object(me, snprintf(arg, 80, "设定玩家%s的徒弟%s为%s。", arg1,
                                 arg2, arg3));
        return 1;
    }

    if (strcmp(arg, "delete") == 0) {
        ptr->del(arg2);
        tell_object(me, snprintf(arg, 255, "删除玩家%s的徒弟%s。", arg1, arg2));

        return 1;
    }

    if (strcmp(arg, "query") == 0) {
        snprintf(arg, 255, "玩家%s徒弟：%s=%ld", arg1, arg2, ptr->query(arg2));
        tell_object(me, arg);
        return 1;
    }

    if (strcmp(arg, "querystr") == 0) {
        snprintf(arg, 255, "玩家%s徒弟：%s=%s", arg1, arg2,
                 ptr->querystr(arg2));
        tell_object(me, arg);
        return 1;
    }

    return 0;
}

int CCmdWizard::reboot(CChar* me, int32_t target, char* arg)
{
    g_channel.do_channel(me, NULL, "news", "世界已经毁灭……请各位好自为之……");
    me->call_out(do_reboot, 1);
    return 1;
}

void CCmdWizard::do_reboot(CContainer* ob, int32_t param1, int32_t param2)
{
    g_reboot = true;
}

// 奖励玩家等级经验
int CCmdWizard::reward_exp(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CChar* who;

    if (!(who = find_player(target))) return notify_fail("没有这个人。");

    int level   = who->query("level");
    int32_t exp = CChar::level2_exp(level + 1) - CChar::level2_exp(level);

    who->add("combat_exp", exp);
    who->flush_my_info();
    who->save();

    tell_object(who,
                snprintf(msg, 255, "$HIY你被系统奖励一级经验了！+%d EXP", exp));
    tell_object(
        me, snprintf(msg, 255, "%s(%s)被%s(%s)奖励了一级经验。(+%d EXP)",
                     who->name(1), who->id(1), me->name(1), me->id(1), exp));

    LOG_INFO("%s", msg);
    return 1;
}

// 扣等级经验
int CCmdWizard::sub_exp(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CChar* who;
    if (!(who = find_player(target))) return notify_fail("没有这个人。");

    int level   = who->query("level");
    int32_t exp = CChar::level2_exp(level) - CChar::level2_exp(level - 1);

    who->add("combat_exp", -exp);
    who->add("level", -1);

    who->flush_my_info();
    who->save();

    tell_object(who,
                snprintf(msg, 255, "$HIY你被系统罚掉一级经验了！-%d EXP", exp));
    tell_object(
        me, snprintf(msg, 255, "%s(%s)被%s(%s)扣掉了一级经验。(-%d EXP)",
                     who->name(1), who->id(1), me->name(1), me->id(1), exp));

    LOG_INFO("%s", msg);
    return 1;
}

int CCmdWizard::print_mapping(CChar* me, int32_t target, char* arg)
{
    printf_all_mappings();
    return 1;
}

// 强制刷新
int CCmdWizard::update(CChar* me, int32_t target, char* arg)
{
    CContainer* who;

    if (!target)
        who = me->environment();
    else if (!(who = get_room_object(me, target))
             && !(who = get_self_object(me, target)))
        return notify_fail("请选中目标。");

    who->load();

    if (who->query_attr(CChar::IS_ROOM)) static_cast<CRoom*>(who)->reset();

    tell_object(me, "OK.");

    return 1;
}

// 热重载 Lua 脚本
// 用法: reload <脚本名>  或  reload all
int CCmdWizard::reload_lua(CChar* me, int32_t target, char* arg)
{
    if (!arg || !arg[0])
        return notify_fail("用法: reload <脚本名> 或 reload all");

    if (cc_strcmp(arg, "all") == 0) {
        // 重载所有已加载脚本
        extern void lua_reload_all();
        lua_reload_all();
        tell_object(me, "所有 Lua 脚本已重载。");
        return 1;
    }

    extern int lua_reload_script(const char* name);
    if (lua_reload_script(arg))
        tell_object(me, "Lua 脚本已重载。");
    else
        tell_object(me, "Lua 脚本重载失败或未加载。");
    return 1;
}

// 强制存盘
int CCmdWizard::save(CChar* me, int32_t target, char* arg)
{
    CContainer* who;

    if (strcmp(arg, "all") == 0) {
        MAP2USER* list = users();
        iterator_user p;

        for (p = list->begin(); p != list->end(); p++) {
            (p->second)->save_to_server();
        }

        tell_object(me, "All players save OK.");
        return 1;
    }

    if (!target)
        who = me->environment();
    else if (!(who = get_room_object(me, target))
             && !(who = get_self_object(me, target))
             && !(who = find_player(target)))
        return notify_fail("请选中目标。");
    who->save();
    tell_object(me, "OK.");
    return 1;
}
