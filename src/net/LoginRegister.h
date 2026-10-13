/**
 * @file LoginRegister.h
 * @brief 注册/名字校验，提供角色创建与合法性检查。
 */

#pragma once

char* check_legal_name(const char* name);
char* make_body(char* name, char* passwd, char* gender, int str, int con,
                int intel, int dex);
int check_valid_char(char* arg);
