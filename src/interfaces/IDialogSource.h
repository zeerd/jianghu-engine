/**
 * @file IDialogSource.h
 * @brief 对话/菜单接口 — 可向玩家展示交互界面的对象。
 */

#pragma once

#include <string>

class CChar;

/**
 * @brief 对话/菜单接口。
 *
 * 实现者：CChar、CItem、CRoom。
 * 调用方：Cmd*.cpp、Lua 脚本。
 */
class IDialogSource {
public:
    virtual ~IDialogSource() = default;

    /**
     * @brief 发送菜单结束标记。
     * @param me  接收菜单的玩家
     */
    virtual void send_menu(CChar* me) = 0;

    /**
     * @brief 添加一个菜单项。
     * @param show  显示文字
     * @param cmd   对应命令
     * @param me    接收菜单的玩家
     */
    virtual void add_menu_item(std::string show, std::string cmd, CChar* me)
        = 0;

    /**
     * @brief 说话 / 显示对话框（小写 say 匹配旧接口）。
     * @param msg   说话内容
     * @param me    对话目标（NULL 时广播）
     * @param tips  对话框标题
     * @return 非零表示已处理
     */
    virtual int say(const char* msg, CChar* me = NULL, const char* tips = 0)
        = 0;
};
