/**
 * @file User.cpp
 * @brief implementation of the CUser class. */

#include <windows.h>

#include "Global.h"
#include "User.h"
#include "Room.h"
#include "GameInit.h"
#include "Channel.h"
#include "RegisterHall.h"
#include "Sql.h"

extern void close_client(uint32_t serial);
extern int send_to_client(int object_id, uint32_t serial, const char* msg);
extern void open_client(uint32_t id, int32_t user);

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------
CUser::CUser()
{
    set("grant", GRANT_PLAYER);
    set("title", "普通百姓");
    memset(m_send_buffer, 0, 512);
    memset(m_command_buffer, 0, sizeof(m_command_buffer));
    m_buffer_len       = 0;
    last_age_set       = 0;
    m_start_idle       = 0;
    m_cmd_buffer_len   = 0;
    m_head_command_ptr = 0;
    m_tail_command_ptr = 0;

    for (auto& cmd : m_command_list) cmd.reset();
}

CUser::~CUser()
{
    for (auto& cmd : m_command_list) cmd.reset();
}

// 是否有可接收 send_command 的客户端连接（连接已建立且未掉线）
bool CUser::has_client()
{
    return querymap_temp("client") && !query_temp("net_dead");
}

void CUser::send_command(const char* msg, int wait)
{
    static char mem[1024];
    static char tmp[512], na[40];
    int comm;

    if (!has_client()) return;
    comm = atol(get_value(msg, "C", mem, 40));
    if ((comm == NOTIFY || comm == COMMON_ATTACK)
        && query_temp("block_msg/all"))
        return;  // 通知消息全部阻塞

    if (query_temp("client/snoop"))  // 被监听状态
    {
        // 在发送前检查，是NOTIFY类型的截获
        if (comm == NOTIFY || comm == COMMON_ATTACK) {
            g_channel.do_snooping(this, get_value(msg, "M", mem, 512));
        }
        else if (comm == TELLOBJECT) {
            g_channel.do_snooping(this,
                                  snprintf(tmp, 400, "$HIG【私聊】%s告诉你：%s",
                                           get_value(msg, "n", na, 40),
                                           get_value(msg, "M", mem, 512)));
        }
        else if (comm == GOTOROOM) {
            g_channel.do_snooping(this,
                                  snprintf(tmp, 400, "$COM你$GRN进入了$COM%s。",
                                           get_value(msg, "N", na, 40)));
        }
        else if (comm == QUESTION) {
            get_value(msg, "s", mem, 255);
            if (mem[0]) {
                g_channel.do_snooping(this, snprintf(tmp, 400, "()%s", mem));
            }
            else {
                get_value(msg, "m", mem, 512);
                if (mem[0]) {
                    g_channel.do_snooping(
                        this, snprintf(tmp, 400, "(%s)%s",
                                       get_value(msg, "n", na, 40), mem));
                }
            }
        }
        else if (comm == NOTIFY_FAIL) {
            g_channel.do_snooping(
                this, snprintf(tmp, 400, "(%s)%s", get_value(msg, "F", na, 40),
                               get_value(msg, "M", mem, 512)));
        }
    }

    strncpy(mem, msg, 1024);
    mem[1023] = 0;

    decode_str(mem);

    if (wait || m_buffer_len) {
        int p = 0;
        // 等待
        while (msg[p]) {
            m_send_buffer[m_buffer_len++] = mem[p++];
            if (m_buffer_len >= 510)  // 512字节一发送
            {
                // 发送处理
                m_send_buffer[m_buffer_len] = 0;
                send_off();
            }
        }
        m_send_buffer[m_buffer_len++] = '\t';
    }
    else {
        // 立即发送：snprintf 已写好并以 NUL 结尾。m_buffer_len 是已用长度
        // （合法区间 0..511，send_off 在其位置上补 NUL），不是容量。
        snprintf(m_send_buffer, 512, "%s\t", mem);
        m_buffer_len = static_cast<int>(strlen(m_send_buffer));
        send_off();
    }
}

int32_t CUser::query_attr(int attr)
{
    switch (attr) {
        case IS_USER:
            return 1;
        case CAN_SAVE:
            return 1;
    }

    return CChar::query_attr(attr);
}

// 设置在线
void CUser::online()
{
    open_client(query_temp("client/serial"), object_id());
    delete_temp("net_dead");

    if (set_online_flag(g_server_id)) set_temp("online", g_server_id);

    on_send_online(this);
}

// 离线 flag = 0 主动断线 flag = 1 被动断线
void CUser::offline(int flag)
{
    LOG_TRACE("IN");

    if (flag) {
        delete_temp("client/serial");
    }

    if (!query_temp("net_dead")) {
        if (!flag) {
            close_client(query_temp("client/serial"));
            delete_temp("client/serial");
        }

        set_temp("net_dead", g_current_time);  // 网掉了。
        delete_temp("client/loading");         // 删掉装入标记

        if (!environment()) return;

        send_state();

        set_heart_beat(1);  // 开始断线计时
    }
    LOG_TRACE("OUT");
}

void CUser::setup()
{
    update_age();
    do_active();

    CChar::setup();

    set_temp("channel_msg_cnt", 8);
    set_temp("save_to_server", g_current_time + 3600);
}

void CUser::send_off()
{
    if (!m_buffer_len) return;
    m_send_buffer[m_buffer_len] = 0;
    send_to_client(object_id(), query_temp("client/serial"), m_send_buffer);
    m_buffer_len = 0;
}

void CUser::update_age()
{
    if (!last_age_set) last_age_set = g_current_time;
    CContainer* env = environment();
    // 全局大厅与它的副本实例同名：站在哪一局都不涨年龄
    if (env && EQUALSTR(env->querystr("base_name"), REGISTER_HALL_NAME)) {
        return;
    }
    add("mud_age", g_current_time - last_age_set);
    last_age_set = g_current_time;
    set("age", g_start_age + query("mud_age") / (172800 / 2));  // 24小时一岁
}

// 用户发呆秒数
int CUser::query_idle() { return g_current_time - m_start_idle; }

void CUser::user_dump()
{
    char msg[255];

    tell_object(
        this, snprintf(msg, 255, "对不起，您已经发呆超过%d分钟了，请下次再来。",
                       IDLE_TIMEOUT / 60));
    tell_room(
        environment(),
        snprintf(msg, 255, "一阵风吹来，将发呆中的%s化为一堆飞灰，消失了。",
                 name()),
        this);

    LOG_INFO("发呆超时踢出: %s(id:%s) 发呆%d秒", name(1), id(1), query_idle());
    command("quit");
}

void CUser::do_active() { m_start_idle = g_current_time; }

// 定位某物件 C=LOCATION, T=目标 BX BY EX EY AC=跑还是走
void CUser::location_object(CContainer* obj)
{
    char msg[512];

    // 非房间，无坐标。不用定位
    if (!(obj->environment())->query_attr(IS_ROOM)) return;

    snprintf(msg, 512, "&C=%ld&T=%ld&bx=%ld&by=%ld&ex=%ld&ey=%ld&ac=%ld&",
             LOCATION, obj->object_id(), obj->query_temp("map/bx"),
             obj->query_temp("map/by"), obj->query_temp("map/ex"),
             obj->query_temp("map/ey"), obj->query_temp("map/ac"));

    send_command(msg, SEND_WAIT_BUFFERED);
}

// 通知用户me在线
void CUser::notify_online(CChar* me)
{
    char msg[255];

    snprintf(msg, 255, "&C=%d&O=1&U=%ld&", SENDUSER, me->object_id());

    send_command(msg, SEND_WAIT_BUFFERED);
}

// 通知用户me的资料 在线玩家信息 倒序 type = 0 NORMAL type = 1 本派 type = 2
// 本帮
void CUser::send_user_online(int begin, int end, int type)
{
    int at_end = 0;
    char msg[512], tmp[40];
    const char* corps_id = querystr("corps/id");
    const char* party_id = querystr("family/family_name");

    MAP2USER* userlist = users();
    iterator_user p;
    CUser* obj;
    int i = 0, count = userlist->size();

    if (!count) return;

    snprintf(msg, 255, "&C=%d&N=%d&", USERLIST, count);

    p = userlist->end();
    while (1) {
        if (at_end) break;

        p--;
        if (p == userlist->begin()) at_end = 1;

        obj = p->second;

        if (obj->query_temp("net_dead")) continue;
        if (type == USERONLINE_PARTY
            && DIFFERSTR(obj->querystr("family/family_name"), party_id))
            continue;

        if (type == USERONLINE_CORPS
            && DIFFERSTR(obj->querystr("corps/id"), corps_id))
            continue;

        if (i >= begin && i < end) {
            snprintf(tmp, 40, "i%d=%ld&", i - begin, obj->object_id());
            strcat(msg, tmp);
        }

        i++;
    }

    // 计算在线人数
    snprintf(tmp, 40, "n=%d&", i);
    strcat(msg, tmp);

    send_command(msg, SEND_WAIT_BUFFERED);
}

// 接受用户详细资料
void CUser::receive_user_info(CUser* me)
{
    char msg[255];

    snprintf(msg, 255, "&C=%d&U=%ld&N=%s&I=%s&r=%s&t=%s&", DOWNUSER,
             me->object_id(), me->name(1), me->id(1), query_rank(me),
             me->title(1));

    send_command(msg, SEND_WAIT_BUFFERED);
}

// 设置准备PK标记
void CUser::ready_pk()
{
    // 设置PK发起标记
    if (!query_temp("pending/PK")) {
        set_temp("pending/PK", g_current_time);

        CContainer* env = environment();
        while (env->environment()) env = env->environment();

        if (env->query("city"))
            set_temp("pending/PK_city", env->querystr("base_name"));
    }
}

// 接收字符串并处理
bool CUser::on_receive(const char* command_string, int ret)
{
    int analyse_ptr   = 0;
    int start         = m_cmd_buffer_len;
    int32_t player_id = object_id();

    while (command_string[analyse_ptr]) {
        if (command_string[analyse_ptr] == '\t')  // Command Line End
        {
            m_command_buffer[m_cmd_buffer_len] = 0;

            decode_str(&m_command_buffer[start]);
            push_command(m_command_buffer, m_cmd_buffer_len + 1);

            // 检查是否已经销毁
            if (g_last_destruct_object == player_id) return true;

            m_cmd_buffer_len = 0;
            start            = 0;
            analyse_ptr++;

            if (analyse_ptr >= ret) return false;
            continue;
        }

        m_command_buffer[m_cmd_buffer_len++] = command_string[analyse_ptr++];

        if (analyse_ptr >= ret || m_cmd_buffer_len >= 1024)  // 越界
        {
            m_cmd_buffer_len
                = m_cmd_buffer_len >= 1024 ? 1024 : m_cmd_buffer_len;
            m_command_buffer[m_cmd_buffer_len] = 0;

            decode_str(&m_command_buffer[start]);
            push_command(m_command_buffer, m_cmd_buffer_len + 1);
            m_cmd_buffer_len = 0;

            LOG_ERROR("接收缓冲越界:用户%ld 字串%s", name(1), m_command_buffer);
            return false;
        }
    }

    // 等待下次处理
    m_command_buffer[m_cmd_buffer_len] = 0;
    decode_str(m_command_buffer);

    return true;
}

void CUser::push_command(char* cmd, int len)
{
    if (m_tail_command_ptr + 1 == m_head_command_ptr
        || m_tail_command_ptr + 1 == m_head_command_ptr + 20) {
        tell_object(this, "对不起，您一次输入的指令太多。");
        return;
    }

    if (m_command_list[m_tail_command_ptr]) {
        LOG_ERROR("用户%s命令泄露! %s.", name(1),
                  m_command_list[m_tail_command_ptr].get());
    }

    // 如果是系统指令，直接执行，不进行排队。
    if (cmd[0] == '0' && cmd[1] == ' ') {
        command(&cmd[2]);
        return;
    }

    m_command_list[m_tail_command_ptr] = std::make_unique<char[]>(len);
    memcpy(m_command_list[m_tail_command_ptr].get(), cmd, len);
    m_command_list[m_tail_command_ptr][len - 1] = 0;

    m_tail_command_ptr++;

    if (m_tail_command_ptr >= 20) m_tail_command_ptr = 0;
}

// 处理当前命令
int CUser::handle_command()
{
    if (m_head_command_ptr == m_tail_command_ptr)  // 无指令
        return 0;
    if (m_head_command_ptr < 0 || m_head_command_ptr > 19)  // 超限
        return 0;
    if (m_tail_command_ptr < 0 || m_tail_command_ptr > 19)  // 超限
        return 0;

    if (!m_command_list[m_head_command_ptr]) {
        LOG_ERROR("用户%s命令队列失败!", name(1));
        return 0;
    }

    int32_t actor;
    char msg[512];

    analyse_string(m_command_list[m_head_command_ptr].get(), msg, 512);
    actor = atol(m_command_list[m_head_command_ptr].get());

    // 释放
    m_command_list[m_head_command_ptr].reset();
    m_head_command_ptr++;

    if (m_head_command_ptr == 20) m_head_command_ptr = 0;

    // 在此判断权限，不可以支配其他人的行为
    if (actor != object_id()) return 0;

    do_active();
    command(msg);

    // 再次查找一次玩家，因为有可能在指令执行时用户被销毁。
    if (g_last_destruct_object != actor) send_off();

    // 未被销毁，在此处发送
    return 1;
}

int CUser::set_online_flag(int online)
{
    // 设置在线标记,防止LOGIN失败的DESTRUCT覆盖连接信息
    if (!online) {
        if (!query_temp("online")) return 0;
        delete_temp("online");
    }

    return db_set_online_flag(id(1), online) ? 1 : 0;
}

// 返回user所在的服务器
int CUser::query_server() { return db_query_server(querystr("id")); }

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:DoActive() - 刷新玩家活动状态（CUser 专用）
int l_do_active(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CUser* user = dynamic_cast<CUser*>(obj);
    if (!user) return 0;

    user->do_active();
    return 0;
}

// self:ready_pk() - 玩家准备PK（CUser 专用）
int l_ready_pk(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CUser* user = dynamic_cast<CUser*>(obj);
    if (!user) return 0;

    user->ready_pk();
    return 0;
}
