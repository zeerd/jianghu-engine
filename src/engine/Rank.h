/**
 * @file Rank.h
 * @brief 称谓查询函数（尊称/蔑称/自称等）。
 */
#pragma once

class CChar;

// --- 称谓 ---
const char* query_rank(CChar* ob);
const char* query_respect(CChar* ob);
const char* query_rude(CChar* ob);
const char* query_self(CChar* ob);
const char* query_self_rude(CChar* ob);
const char* query_close(CChar* me, CChar* ob);
const char* query_self_close(CChar* me, CChar* ob);
