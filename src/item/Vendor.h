/**
 * @file Vendor.h
 * @brief 商人 NPC，管理商品列表与买卖操作。
 */

#pragma once

#include <memory>

#include "Npc.h"

#define VENDOR_BEGIN(npc_name)        \
    class npc_name : public CVendor { \
    public:                           \
        npc_name() {};                \
        virtual ~npc_name() {};
#define VENDOR_END \
    }              \
    ;

struct DTGood {
    int32_t m_code  = 0;
    char m_name[40] = "";  // 名称
    string m_basename;     // 物件类
    int m_index      = 0;  // 类内索引
    int32_t m_amount = 0;  // 数量
    int32_t m_value  = 0;  // 价值
};

typedef std::list<std::unique_ptr<DTGood>> DTGOODLIST;

/**
 * @brief 商人 NPC，管理商品列表并处理购买/出售请求。
 */
class CVendor : public CNpc {
public:
    void carry_good(const char* good, int32_t amount = 1, int index = 0);
    virtual int handle_action(char* comm, CChar* me, char* arg);
    int do_buy(CChar* me, int32_t obj);
    int do_sell(CChar* me, CContainer* obj);
    int do_list(CChar* me, int page = 0);
    virtual int do_action(int action, CChar* me = NULL);
    virtual int32_t query_attr(int attr);
    CVendor();
    virtual ~CVendor();

protected:  // 商人，买卖物品。
    DTGood* get_good(int32_t good);
    DTGood* find_good(string basename, int index = 0);
    void add_good(CContainer* good, int32_t amount = 1);
    DTGOODLIST m_good_list;  // 商品列表
    int32_t m_good_code;     // 商品编码

    int m_vendor_type;
};
