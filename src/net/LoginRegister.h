/**
 * @file LoginRegister.h
 * @brief 注册/名字校验，提供角色创建与合法性检查。
 */

#pragma once

class CUser;

// 新角色起手值（起手经验固定为 0，不在此列）。起手年龄不在这里——
// 建档（init_new_char）与注册行（db_register_account）统一取 config
// [server] start_age（g_start_age），改配置即两处同改，勿再各写一套。
constexpr int NEW_CHAR_LEVEL = 1;

char* check_legal_name(const char* name);
// 新角色初始档案：由首次登录的新人分支（verify_user 的建档分支）调用，
// 把起手值集中在一处，保证建档路径不再各写一套。
void init_new_char(CUser& me, const char* name, const char* id,
                   const char* gender, int str, int con, int intel, int dex);
int check_valid_char(char* arg);
