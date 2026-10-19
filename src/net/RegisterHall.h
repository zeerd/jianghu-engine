/**
 * @file RegisterHall.h
 * @brief 注册大厅副本化：玩家各自进独立的大厅实例。
 */

#pragma once

class CChar;

// 副本总控脚本名（lua-default/room/rk_fb.lua，type="fb"）
inline constexpr const char* REGISTER_FB_NAME = "rk_fb";

// 注册大厅房名（lua-default/room/rk.lua）。副本房与全局大厅同名同脚本，
// 故注册白名单、注册专员、update_age 的大厅判定对实例一并生效。
inline constexpr const char* REGISTER_HALL_NAME = "rk";

// 把 me 送进一个独立的注册大厅副本实例；实例不可用时回落全局大厅。
void enter_register_hall(CChar* me);
