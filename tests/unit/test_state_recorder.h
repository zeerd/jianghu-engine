// test_state_recorder.h —
// 记录发往客户端的协议命令，用于断言状态包的投递/广播抑制。
//
// 自包含：仅依赖 User.h（CUser 及 IClientObject/Global.h 经其传递包含）。
// 通过重写 send_command
// 捕获所有定向投递（私发）与房间内广播（message_vision_room）。
//
#pragma once

#include <string>
#include <vector>

#include "User.h"

class StateRecordingUser : public CUser {
public:
    void send_command(const char* msg, int wait = SEND_WAIT_NONE) override
    {
        ++sent;
        if (msg) msgs.emplace_back(msg);
    }

    // 任一已发送消息包含 needle 则返回真。
    bool has(const char* needle) const
    {
        for (const auto& m : msgs) {
            if (m.find(needle) != std::string::npos) return true;
        }
        return false;
    }

    // 清空已记录投递——用于在 move() 等准备动作之后隔离出被测调用的效果。
    void clear()
    {
        sent = 0;
        msgs.clear();
    }

    int sent = 0;
    std::vector<std::string> msgs;
};
