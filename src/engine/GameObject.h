/**
 * @file GameObject.h
 * @brief 跨类型通用对象管理（查找/销毁/物品/角色/战斗）。
 */
#pragma once

#include <cstdint>

class CContainer;
class CChar;
class CFightRoom;

typedef CContainer* PCContainer;

// --- 通用查找 ---
CContainer* find_object(int32_t id, int flag = 0);

// --- 对象销毁 ---
void destruct(PCContainer me);
void destruct_with_logs(PCContainer me, const char* function = "");

// --- 物品操作 ---
CContainer* random_get_item(CContainer* me, int type,
                            CContainer* exclude = NULL);

// --- 角色查询 ---
int is_living(CContainer* me);
int wiz_level(CChar* me);

// --- 地图实时位置（客户端地图同步用的 map/* 临时坐标） ---
// 收拢到一个点：起点与终点重合、移动标志归零（重连定位、丢弃落点、战斗精灵继承）。
void set_live_point(CContainer* obj, int32_t x, int32_t y);
// 只改终点：保留当前起点与移动标志（切磋选点、比试继承当前终点）。
void set_live_dest(CContainer* obj, int32_t x, int32_t y);

// --- 战斗 ---
CFightRoom* do_bishi(CChar* me, CChar* victim, char* name, int check = 50);
