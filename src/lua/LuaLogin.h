/**
 * @file LuaLogin.h
 * @brief 登录后 Lua 脚本校验。
 */
#pragma once

class CChar;

// 返回值: 0=校验通过, 1=校验失败, -1=脚本加载/执行错误
int check_player(CChar* me, int newenter = 1);
