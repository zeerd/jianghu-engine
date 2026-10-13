/**
 * @file IActionHandler.h
 * @brief 动作分派接口 — 可拦截玩家命令的对象。
 */

#pragma once

class CChar;

/**
 * @brief 动作分派接口。
 *
 * 实现者：CChar、CItem、CRoom。
 * 调用方：CmdHandle.cpp 命令分发器。
 */
class IActionHandler {
public:
    virtual ~IActionHandler() = default;

    /**
     * @brief 执行装备/使用等动作。
     * @param nAct  动作枚举值
     * @param me    操作者
     * @return 非零表示已处理
     */
    virtual int do_action(int act, CChar* me = NULL) = 0;

    /**
     * @brief 三参数版命令拦截（无目标）。
     * @param comm  命令名
     * @param me    发令者
     * @param arg   参数
     * @return 非零表示已处理
     */
    virtual int handle_action(char* comm, CChar* me, char* arg) = 0;

    /**
     * @brief 四参数版命令拦截（含目标）。
     * @param comm   命令名
     * @param me     发令者
     * @param arg    参数
     * @param tar    目标 ID 字符串
     * @return 非零表示已处理
     */
    virtual int handle_action(char* comm, CChar* me, char* arg, char* tar) = 0;
};
