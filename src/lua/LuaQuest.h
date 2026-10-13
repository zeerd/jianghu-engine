/**
 * @file LuaQuest.h
 * @brief 通用任务钩子的 C++ → Lua 薄转发层。
 */

//---------------------------------------------------------

#pragma once

class CChar;
class CContainer;
class CNpc;

// 通用谓词：某玩家身上是否存在进行中的任务。
// 具体用哪个 key、任务系统是否存在，全部由 Lua 侧 quest_mgr.has_quest 决定；
// quest_mgr 不存在（require 失败）时返回 false，C++ 不假设任何具体任务系统。
bool quest_has_active(CChar* me);

// CNpc::accept_object 转发，返回 1 表示已处理。
int quest_accept_object(CNpc* npc, CChar* me, CContainer* item);

// CNpc::do_talk 的对话事件转发。
// 返回 true 表示任务分支已终止对话；false 表示应继续询问菜单逻辑。
bool quest_do_talk(CNpc* npc, CChar* me, char* ask);

// NPC 被玩家击杀事件转发（任务完成/掉落判定由 Lua 负责）。
void quest_npc_killed(CChar* killer, CChar* victim);
