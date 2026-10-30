/**
 * @file logind.cpp
 * @brief 登录主流程 */

#include "Global.h"
#include "User.h"
#include "Npc.h"
#include "Room.h"
#include "GameInit.h"
#include "CmdHandle.h"
#include "Channel.h"
#include "Skill.h"
#include "Combatd.h"
#include "CombatConfig.h"
#include "Money.h"
#include "LoginAuth.h"
#include "LuaLogin.h"
#include "LoginRegister.h"
#include "RegisterHall.h"
#include "../lua/LuaEngine.h"

#include "Sql.h"

int g_start_pk = 1;

extern char g_notify_fail_msg[512];

extern void close_client(uint32_t serial);
extern int send_to_client(int object_id, uint32_t serial, const char* msg);
extern int send_to_client_direct(int sockfd, uint32_t serial, const char* msg);

const char* query_rank(CChar* ob);

bool verify_user(const char* userid, char* passwd);

// 发布用户上线信息
void on_send_online(CUser* me)
{
    // 通知自己上线状态
    me->notify_online(me);

    // 花名册交换（/t 玩家数字 ID 全服跨房间解析的依据）：直接服务端互推
    // DOWNUSER（U/N/I 字段：会话号/姓名/数字账号）写入双方客户端在线表——
    // 不走 SENDUSER→downloaduser 往返，避免批量登录时的 downloaduser 命令
    // 突发撞上 per-user 限流。任何登录次序下，登录完成即互相可见；
    // 下线由 CUser::notify_offline 广播 SENDUSER O=0 按 UID 摘除。
    for (iterator_user p = g_user_list.begin(); p != g_user_list.end(); p++) {
        CUser* obj = p->second;
        if (obj == me) continue;
        if (obj->query_temp("net_dead")) continue;

        obj->receive_user_info(me);  // 对端在线表 ← 本人资料
        me->receive_user_info(obj);  // 本人在线表 ← 对端资料
    }
}

// 通过帐号查找在线玩家
CUser* find_online(const char* id)
{
    iterator_user p;
    CUser* obj;

    for (p = g_user_list.begin(); p != g_user_list.end(); p++) {
        obj = p->second;
        if (EQUALSTR(obj->querystr("id"), id)) return obj;
    }

    return 0;
}

// 游客账号计数器：每次游客登录递增，账号取其相反数（负数），
// 与数据库中的正式账号（恒为正数）天然不冲突。
int g_guest_account = 0;

// 游客通行证：guest/guest 免注册进入游戏大厅（rk），
// 在大厅找注册专员创建正式角色。nid/passwd 精确匹配（区分大小写）。
bool is_guest_login(const char* nid, const char* passwd)
{
    return nid && passwd && strcmp(nid, "guest") == 0
           && strcmp(passwd, "guest") == 0;
}

// 分配一个游客临时账号 id（负数）
long alloc_guest_account() { return -(++g_guest_account); }

// 游客临时角色：无档案、不落库，只填最少可上线字段。
// 起始地记为游戏大厅 rk（无环境时的重连兜底），首次上线则由
// enter_register_hall 送进各自的副本实例；正式角色由大厅的注册专员创建。
void setup_guest_user(CUser* me)
{
    // set_name 的第二参数会写回 dbase"id"：必须先拷贝出独立缓冲，
    // 直接把 me->id(1) 传进去会与自身 dbase 存储别名导致悬垂指针。
    char gid[24];
    snprintf(gid, sizeof(gid), "%s", me->id(1));

    me->set_name("游客", gid);
    me->set("gender", "男性");
    me->set("age", g_start_age);
    me->set("str", 10);
    me->set("int", 10);
    me->set("con", 10);
    me->set("dex", 10);
    me->set("level", 1);
    me->set("combat_exp", 0);
    me->set("title", "初来乍到");
    me->set("guest", 1);
    me->set("start_city", REGISTER_HALL_NAME);
}

void send_info_to_client(CUser* me)
{
    DTItemList* t;
    CContainer *obj, *env;

    env = me->environment();

    // 向客户端发送信息
    me->flush_my_info();  // 发送角色基本信息
    me->flush_my_skill(me);
    me->send_state();

    // 重新发布携带物品信息
    t = me->get_item_list();
    for (auto val : *t) {
        obj = val;
        me->on_add_object(obj, me);
    }

    me->flush_my_equip();
}

// 发布公告信息
void down_bullet_info(CChar* me)
{
    char msg[512];

    // 发送公告开始标记
    snprintf(msg, 512, "&C=%d&B=%d&", BULLET, PROTO_FIELD_BULLET);
    me->send_command(msg, SEND_WAIT_BUFFERED);

    // 从 Lua 获取公告内容
    if (g_LuaEngine) {
        std::vector<std::string> lines;
        if (g_LuaEngine->get_bullet_lines(lines)) {
            for (const auto& line : lines) {
                if (!line.empty()) {
                    snprintf(msg, 512, "&C=%d&M=%s&", BULLET, line.c_str());
                    me->send_command(msg, SEND_WAIT_BUFFERED);
                }
            }
        }
        else {
            LOG_ERROR("DownBulletInfo: Failed to load bullet lines from Lua");
        }
    }
    else {
        LOG_ERROR("DownBulletInfo: Lua engine not initialized");
    }
}

int on_login(uint32_t serial, int sockfd, char* arg)
{
    CUser* me;
    char passwd[255];
    char local_ip[512], ipaddress[80];
    char cardno[80], cardpass[80];
    char msg[512], version_info[255];
    char nid[255], id[255], fmt1[80], fmt2[80];
    const char* myip;
    double left = 0;

    snprintf(ipaddress, 80, "%s", &arg[255]);

    decode_str(arg);

    replace_string(arg, "\'", "", 512);

    LOG_DEBUG("%s %s", arg, ipaddress);
    analyse_string(arg, local_ip, 512);

    if (strcmp(arg, "login")) {
        snprintf(msg, 255, "&C=%d&F=%d&M=请到主页注册&\t", NOTIFY,
                 PROTO_FIELD_ERROR);

        send_to_client_direct(sockfd, serial, decode_str(msg));

        return false;  // 非登录指令
    }

    analyse_string(local_ip, version_info, 255);
    analyse_string(version_info, nid, 255);
    analyse_string(nid, passwd, 255);
    analyse_string(passwd, cardno, 80);
    analyse_string(cardno, cardpass, 40);

    // 游客通行证（guest/guest）：免注册、免查库，直接进游戏大厅
    bool guest_login = is_guest_login(nid, passwd);

    try {
        if (guest_login) {
            snprintf(id, 20, "%ld", alloc_guest_account());
        }
        else {
            snprintf(id, 20, "%ld", atol(nid));
        }
    } catch (...) {
        LOG_ERROR("error id:%s", nid);
        notify_fail("未知的id！");
        return false;
    }

    notify_fail("您的密码不正确！");

    if (!guest_login && !verify_user(id, passwd)) {
        snprintf(msg, 255, "&C=%d&F=%d&M=%s&\t", LOGINUSER, PROTO_FIELD_ERROR,
                 g_notify_fail_msg);

        send_to_client_direct(sockfd, serial, decode_str(msg));
        return false;
    }

    // 检查帐号是否被封（游客无正式帐号，跳过）
    if (!guest_login && query_invalid_account(id)) {
        snprintf(msg, 255, "&C=%d&F=%d&M=%s&\t", LOGINUSER, PROTO_FIELD_ERROR,
                 "对不起，此帐号停止使用！");

        send_to_client_direct(sockfd, serial, decode_str(msg));
        return false;
    }

    // now arg is username
    bool newenter = true;
    me = find_online(id);  // 通过帐号查找在线用户，有可能改为ID是数字帐号的。
    if (me)                // 已经在线，将把原来的连接断掉，取代之。
    {
        // 判断是否原来的连接
        snprintf(
            msg, 255,
            "&C=%d&M=有人从别处(M:%s L:%s)进入取代了你。请重新登录！&",  // 断线通知
            NOTIFY, ipaddress, local_ip);
        static_cast<CUser*>(me)->send_command(msg);

        // 记入名单
        g_channel.loginlist.add(get_ip(me), -1);

        // 关闭原来的通讯
        me->set("last_quit_time", g_current_time);
        LOG_INFO("重复登录替换旧连接: %s(id:%s)", me->name(1), me->id(1));
        me->offline();

        me->delete_temp("client/loading");

        me->set_temp("client/serial", serial);
        me->set_temp("client/sockfd", sockfd);
        me->set_temp("client/version", version_info);  // 设置客户端版本

        me->set_temp("client/ip_address", ipaddress);
        me->set_temp("client/ip_local", local_ip);

        myip = get_ip(me);
        // 记入名单
        g_channel.loginlist.add(myip, 1);

        // 检查IP地址
        int loginlimit = g_channel.loginlimit.query(myip);
        if (!loginlimit) loginlimit = g_combat_config.ip_limit();

        if (g_channel.loginlist.query(myip) > loginlimit) {
            snprintf(msg, 255,
                     "&C=%d&F=%d&M=对不起，该地址被限制为不得登陆超过%"
                     "d个帐号！&\t",
                     LOGINUSER, PROTO_FIELD_ERROR, loginlimit);

            send_to_client_direct(sockfd, serial, decode_str(msg));
            return false;
        }

        // 检测用户所在的ＳＥＲＶＥＲ
        int server = me->query_server();
        if (server && server != g_server_id) {
            snprintf(msg, 255,
                     "&C=%d&F=%d&M=对不起，该用户现在%d号服务器上！&\t",
                     LOGINUSER, PROTO_FIELD_ERROR, server);

            send_to_client_direct(sockfd, serial, decode_str(msg));
            return false;
        }

        // 与通讯部分取得连接
        me->online();

        // 切换场景信息
        CContainer* env = me->environment();
        if (!env)  // 处于无环境的玩家，移入起始城市
        {
            CRoom* room = load_room(me->querystr("start_city"));
            me->move(room);  // 用户切入场景
        }
        else {
            env->change_room(me);
        }

        strncpy(fmt1, "$N重新连线进入这个世界。", 80);
        strncpy(fmt2, "%s从(%s)重新连线完毕。", 80);
        newenter = false;
    }
    else  // 未在线则登录
    {
        MAP2USER* ulist = users();
        int32_t count   = ulist->size();
        if (count >= g_combat_config.login_limit()) {
            snprintf(
                msg, 255, "&C=%d&F=%d&M=对不起，游戏限制为%ld，请稍候再连。&\t",
                LOGINUSER, PROTO_FIELD_ERROR, g_combat_config.login_limit());

            send_to_client_direct(sockfd, serial, decode_str(msg));
            return false;
        }

        me = add_user();

        me->set("id", id);  // 设置帐号

        me->set_temp("client/serial", serial);
        me->set_temp("client/sockfd", sockfd);
        me->set_temp("client/version", version_info);  // 设置客户端版本

        me->set_temp("client/ip_address", ipaddress);
        me->set_temp("client/ip_local", local_ip);

        myip = get_ip(me);

        // 记入名单
        g_channel.loginlist.add(myip, 1);

        if (ban_me(me)) {
            snprintf(msg, 255,
                     "&C=%d&F=%d&M=对不起，您所在的地址在江湖中的声誉被破坏！&"
                     "\t",
                     LOGINUSER, PROTO_FIELD_ERROR);

            send_to_client_direct(sockfd, serial, decode_str(msg));

            DESTRUCT_OB(me, "Login");
            return false;
        }

        // 检查IP地址
        int loginlimit = g_channel.loginlimit.query(myip);
        if (!loginlimit) loginlimit = g_combat_config.ip_limit();

        if (g_channel.loginlist.query(myip) > loginlimit) {
            snprintf(msg, 255,
                     "&C=%d&F=%d&M=对不起，该地址被限制为不得登陆超过%"
                     "d个帐号！&\t",
                     LOGINUSER, PROTO_FIELD_ERROR, loginlimit);

            send_to_client_direct(sockfd, serial, decode_str(msg));
            DESTRUCT_OB(me, "Login");
            return false;
        }

        if (guest_login) {
            // 游客无档案：造一个临时角色直接进游戏大厅
            setup_guest_user(me);
        }
        else if (!me->load_from_server()) {
            snprintf(msg, 255,
                     "&C=%d&M=对不起，您的档案不存在，请重新申请。&\t", NOTIFY);

            send_to_client_direct(sockfd, serial, decode_str(msg));
            DESTRUCT_OB(me, "Login");
            return false;
        }

        // 检测用户所在的ＳＥＲＶＥＲ（游客不在库中，跳过）
        int server = guest_login ? 0 : me->query_server();
        if (server && server != g_server_id) {
            snprintf(msg, 255,
                     "&C=%d&F=%d&M=对不起，该用户现在%d号服务器上！&\t",
                     LOGINUSER, PROTO_FIELD_ERROR, server);

            send_to_client_direct(sockfd, serial, decode_str(msg));

            DESTRUCT_OB(me, "Login");
            return false;
        }

        // 与通讯部分取得连接
        me->online();
        me->setup();

        // 频道设置
        CVector v;
        v.append("chat");
        v.append("rumor");
        v.append("party");
        v.append("corps");
        v.append("ad");
        v.append("answer");

        me->set("channels", v);
        CRoom* room = nullptr;

        if (guest_login) {
            // 游客进自己的注册副本实例（入口内含不可用时的全局大厅兜底）
            enter_register_hall(me);
        }
        else if (me->querystr("start_obj")[0]) {
            room = load_room(me->querystr("start_obj"));
            me->del("start_obj");
        }
        else {
            room = load_room(me->querystr("start_city"));

            if (room->query("fail_ob")
                || (EQUALSTR(room->querystr("area"), "house")
                    && !EQUALSTR(room->querystr("owner"), me->querystr("id"))
                    && !EQUALSTR(room->querystr("second_owner"),
                                 me->querystr("id")))
                || (room->query("corps")
                    && !EQUALSTR(room->querystr("corps_id"),
                                 me->querystr("corps/id")))) {
                room = load_room(g_start_room);
                me->set("start_city", g_start_room);
            }
        }
        if (room) me->move(room);  // 用户切入场景（游客已在副本实例里）

        if (guest_login) {
            tell_object(
                me,
                "注册专员冲你招了招手：这位少侠，想闯荡江湖，先来找我注册一"
                "个正式角色吧！（输入 register 即可开始注册）");
        }

        strncpy(fmt1, "$N连线进入这个世界。", 80);
        strncpy(fmt2, "%s从(%s)连线进入这个世界。", 80);
    }

    // 返回签到结果，包括版本信息
    time_t current;

    snprintf(msg, 255,
             "&C=%d&U=%d&G=%d&V=%s&R=%d&M=%s&l=%d&Y=%d&",  // PLAYER or WIZARD
             LOGINUSER, me->object_id(), me->query("grant"), MAIN_VERSION,
             (int32_t)difftime(g_current_time, g_boot_time), MAIN_NAME,
             (int32_t)(left * 10), me->query_temp("charge/month"));
    me->send_command(msg, SEND_WAIT_BUFFERED);

    // 下载公告信息
    down_bullet_info(me);

    send_info_to_client(me);

    ////取得上次连线地址------------------------------------------------------------
    string lastip = me->querystr("login/ip");
    current       = me->query("login/time");

    struct tm* newtime;
    newtime = localtime(&current);

    sprintf(msg, "$HIG上次连线 %d/%02d/%02d %02d:%02d:%02d 来自地址 %s",
            1900 + newtime->tm_year, newtime->tm_mon + 1, newtime->tm_mday,
            newtime->tm_hour, newtime->tm_min, newtime->tm_sec, lastip.c_str());

    tell_object(me, msg);

    time(&current);
    newtime = localtime(&current);
    sprintf(msg, "$HIG本次连线 %d/%02d/%02d %02d:%02d:%02d 来自地址 %s\n",
            1900 + newtime->tm_year, newtime->tm_mon + 1, newtime->tm_mday,
            newtime->tm_hour, newtime->tm_min, newtime->tm_sec, myip);

    tell_object(me, msg);

    me->set("login/time", static_cast<int32_t>(current));
    me->set("login/ip", myip);
    //-----------------------------------------------------------------------------

    message_vision(fmt1, me);

    snprintf(msg, 255, fmt2, me->name(1), get_ip(me));
    g_channel.do_channel(&g_channel, 0, "sys", msg);

    LOG_INFO("%s", msg);

    // 下载emote信息
    g_channel.download_emote(me);

    // 登录校验与奖励（login.on_login）：返回 2 = 拒绝准入，
    // 引擎只执行通用机制（送回游戏大厅并断开），原因与文案由脚本自理
    if (check_player(me, newenter) == 2) {
        enter_register_hall(me);
        me->send_off();
        return true;
    }

    // 数据存到本地（游客不落库）
    if (!guest_login) me->save();

    me->send_off();

    return true;
}

bool splite_command_parse(long userid, char* command_string, int ret)
{
    char msg[255];
    CUser* me = find_player(userid);

    if (!me) {
        g_channel.do_channel(
            &g_channel, 0, "sys",
            snprintf(msg, 255, "指令丢失：用户%ld不存在！", userid));
        LOG_ERROR("lost: user %ld cmd %s", userid, decode_str(command_string));
        return false;
    }

    decode_str(command_string);
    me->push_command(command_string, ret);

    return true;
}

void on_offline(long objectid)
{
    CUser* me;
    me = find_player(objectid);
    if (!me) return;

    LOG_INFO("网络断线: %s(id:%s) IP=%s", me->name(1), me->id(1), get_ip(me));
    me->offline(1);

    char msg[80];
    g_channel.do_channel(&g_channel, 0, "sys",
                         snprintf(msg, 80, "%s断线了。", me->name(1)));
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// GetIP(ob) -> string - 获取角色 IP 地址
int l_get_ip(lua_State* L)
{
    CChar* ch = NULL;
    if (lua_isuserdata(L, 1)) ch = dynamic_cast<CChar*>(get_object(L, 1));
    const char* s = (ch ? get_ip(ch) : "");
    lua_pushstring(L, s ? s : "");
    return 1;
}

// find_online(id) -> obj | nil - 按 id 查找在线玩家
int l_find_online(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    CUser* user    = find_online(id);
    lua_bind_push_object(L, user);
    return 1;
}

// get_all_online_users() -> table - 返回所有在线玩家对象的数组（下标从1开始）
int l_get_all_online_users(lua_State* L)
{
    int cnt = 0;
    for (iterator_user p = g_user_list.begin(); p != g_user_list.end(); p++)
        cnt++;
    lua_createtable(L, cnt, 0);
    int idx = 1;
    for (iterator_user p = g_user_list.begin(); p != g_user_list.end(); p++) {
        lua_bind_push_object(L, p->second);
        lua_rawseti(L, -2, idx);
        idx++;
    }
    return 1;
}
