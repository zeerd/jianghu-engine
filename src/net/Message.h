/**
 * @file Message.h
 * @brief 消息输出函数，提供 tell_object/tell_room/message_channel 等接口。
 */

#pragma once

#include "../object/Global.h"

#include <string>

// --- 字符串工具 ---
// 就地替换：结果截到 len-1（字段宽度是协议契约）。std::string 形态是唯一
// 算法实现；char* 形态由它代写，保留给尚未迁移到字符串管线的调用点。
void replace_string(std::string& msg, const char* oldstr, const char* newstr,
                    size_t len);
void replace_string(char* msg, const char* oldstr, const char* newstr, int len);
char* chinese_number(mapping_value_t i, char* msg);

// --- 称谓 ---
const char* gender_self(string sex);
const char* gender_pronoun(string sex);

// --- 消息输出 ---
void tell_object(CContainer* ob, const char* str,
                 const char* channel = "system");
void tell_room(CContainer* ob, const char* str, CChar* exclude = NULL,
               const char* channel = "system");
void message_channel(const char* channel, const char* msg, CChar* me,
                     CContainer* you = NULL);
void message_vision(const char* msg, CContainer* me, CContainer* you = NULL,
                    bool self = false);
void message(const char* channel, const char* chat, MAP2USER* ulist,
             CContainer* me = NULL, CContainer* who = NULL);
void message_vision_room(CContainer* me, const char* room, const char* chatmsg,
                         CContainer* exclude  = NULL,
                         CContainer* exclude2 = NULL);
