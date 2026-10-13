/**
 * @file HeartBeat.h
 * @brief 心跳系统，管理延迟销毁列表与心跳调度。
 */
#pragma once

#include <cstdint>
#include <list>
#include <mutex>

class CContainer;

// --- 延迟销毁列表 ---
extern std::list<CContainer*> g_destruct_list;
extern std::mutex g_destruct_list_mutex;

// 冲刷延迟销毁列表：停止心跳、脱离容器并销毁
void flush_destruct_list();

// --- 心跳管理 ---
void set_heart_beat_obj(CContainer* obj, int flag);
void call_heart_beat();
