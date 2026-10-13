/**
 * @file CmdHandle.h
 * @brief 命令分发处理核心，定义命令宏与通用工具函数。
 */

#pragma once

#include "User.h"

void drop_all(CChar* me);
int notify_fail(const char* fmt, ...);
int check_ap(int ap, CChar* me);
void resume_ap(int ap, CChar* me);
int is_relative(CChar* me, CChar* who);
CChar* get_room_char(CChar* me, int32_t target);
CContainer* get_room_object(CChar* me, int32_t target);
CContainer* get_self_object(CChar* me, int32_t target);

// 命令处理
class CCmdHandle {
public:
    int command(char* Comm, CChar* me);
    CCmdHandle();
    virtual ~CCmdHandle();

protected:
    static void do_revive(CContainer* ob, int32_t param1, int32_t param2);
    static void do_kick(CContainer* me, int32_t param1, int32_t param2);
    int handlecommand(char* cmd, CChar* me, int32_t target, char* arg);
};
