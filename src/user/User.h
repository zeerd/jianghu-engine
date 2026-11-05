/**
 * @file User.h
 * @brief 玩家类，管理在线状态/命令处理/发送缓冲。
 */

#pragma once

#include <array>
#include <memory>

#include "Char.h"
#include "ConnectionHandleManager.h"

#define USERONLINE_NORMAL 0
#define USERONLINE_PARTY  1
#define USERONLINE_CORPS  2

/**
 * @brief 玩家类，管理在线状态、命令队列与发送缓冲区。
 */
class CUser : public CChar {
public:
    int set_online_flag(int online);

    int query_server();
    int handle_command();
    void push_command(char* command, int len);

    void ready_pk();
    void receive_user_info(CUser* me);
    void send_user_online(int begin, int end, int type = 0);
    void notify_online(CChar* me);
    void notify_offline();

    void location_object(CContainer* obj);
    void do_active();
    void user_dump();
    int query_idle();
    void update_age();
    virtual void send_off();
    virtual void setup();
    void offline(int flag = 0);
    void online();
    // 连接句柄（见 CConnectionHandleManager）：登录受理时绑定、断线时清零，
    // 0 = 无连接；发送/关闭凭它查表寻址连接对象
    void set_connection_handle(CConnectionHandleManager::Handle handle);
    virtual int32_t query_attr(int attr);
    virtual void send_command(const char* msg, int wait = SEND_WAIT_NONE);
    bool has_client() override;

    CUser();
    virtual ~CUser();

protected:
    CConnectionHandleManager::Handle m_connection_handle
        = 0;  // 连接句柄，0=无连接
    int m_start_idle;
    int last_age_set;
    char m_send_buffer[512];  // 发送缓冲区
    int m_buffer_len;         // 缓冲区已用长度（0..511，末尾留位给 NUL）

    int m_cmd_buffer_len;         // 缓冲区已用长度（0..1024，末尾留位给 NUL）
    char m_command_buffer[1025];  // 接收缓冲区

    std::array<std::unique_ptr<char[]>, 20> m_command_list;  // 等待处理命令
    int m_tail_command_ptr;  // 当前处理指令队列尾
    int m_head_command_ptr;  // 当前处理指令队列头
};

CUser* find_online(const char* id);
