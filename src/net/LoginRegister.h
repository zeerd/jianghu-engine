/**
 * @file LoginRegister.h
 * @brief 注册/名字校验，提供角色创建与合法性检查。
 */

#pragma once

class CUser;

// 新角色起手值：init_new_char（建档）与注册 INSERT（账号行）共用一份，
// 避免两处各写一套而漂移。起手经验为 0，不在此列。
constexpr int NEW_CHAR_AGE   = 14;
constexpr int NEW_CHAR_LEVEL = 1;

char* check_legal_name(const char* name);
// 新角色初始档案：make_body 与首次登录的新人分支（verify_user）共用一份，
// 保证两条入口建出来的角色起手值完全一致。
void init_new_char(CUser& me, const char* name, const char* id,
                   const char* gender, int str, int con, int intel, int dex);
int check_valid_char(char* arg);
