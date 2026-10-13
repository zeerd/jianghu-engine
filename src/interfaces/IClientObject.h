/**
 * @file IClientObject.h
 * @brief 客户端序列化接口 — 可向客户端发送自身数据的对象。
 */

#pragma once

#include <cstdint>

class CChar;

/**
 * @brief 客户端序列化接口。
 *
 * 实现者：CChar、CItem、CRoom。
 * 调用方：CmdStandardLife.cpp、ContainerMove.cpp、Lua 脚本。
 */
class IClientObject {
public:
    virtual ~IClientObject() = default;

    /**
     * @brief 向客户端发送完整物件数据。
     * @param me  接收数据的玩家
     */
    virtual void download(CChar* me) = 0;

    /**
     * @brief 将物件属性序列化到字符串。
     * @param str  输出缓冲区（追加模式）
     * @param me   请求查看的玩家
     */
    virtual void get_info(char* str, CChar* me) = 0;

    /**
     * @brief 玩家查看此物件时的响应。
     * @param me  查看者
     * @return 非零表示已处理（不再显示默认菜单）
     */
    virtual int do_look(CChar* me) = 0;

    /**
     * @brief 向客户端发送协议命令。
     * @param msg   协议字符串
     * @param wait  发送等待模式
     */
    virtual void send_command(const char* msg, int wait = 0) = 0;
    /**
     * 当前是否真有可接收 send_command 数据的客户端连接。
     *
     * 这是「投递能力」而非身份判断：默认 false（send_command 的空实现方
     * 保持默认），由真正持有连接的子类覆写。调用方据此决定是否值得先拼装
     * 消息——否则投递必然是空操作。
     */
    virtual bool has_client() { return false; }
};
