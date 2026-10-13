/**
 * @file Item.h
 * @brief 物品基类，管理移动/叠加/使用/拆分。
 */

#pragma once

#include "Char.h"
#include "IHittable.h"
#include "IEquippable.h"
#include "IActionHandler.h"
#include "IDialogSource.h"
#include "IClientObject.h"

#define ITEM_BEGIN(item_name)        \
    class item_name : public CItem { \
    public:                          \
        item_name() {};              \
        virtual ~item_name() {};
#define ITEM_END \
    }            \
    ;

/**
 * @brief 物品基类，支持堆叠、移动、使用与拆分。
 *
 * 实现接口：IHittable, IEquippable, IActionHandler, IDialogSource,
 * IClientObject。
 */
class CItem : public CContainer,
              public IHittable,
              public IEquippable,
              public IActionHandler,
              public IDialogSource,
              public IClientObject {
public:
    // === IHittable ===
    const char* hit_ob(CChar* me, CChar* victim, int32_t damage_bonus) override;
    void owner_is_killed(CChar* killer) override;

    // === IEquippable ===
    int wear() override;
    int wield() override;

    // === IActionHandler ===
    int do_action(int act, CChar* me = NULL) override;
    int handle_action(char* comm, CChar* me, char* arg) override;
    int handle_action(char* comm, CChar* me, char* arg, char* tar) override;

    // === IDialogSource ===
    void send_menu(CChar* me) override;
    void add_menu_item(string show, string cmd, CChar* me) override;
    int say(const char* msg, CChar* me = NULL, const char* tips = 0) override;

    // === IClientObject ===
    void download(CChar* me) override;
    void get_info(char* str, CChar* me) override;
    int do_look(CChar* me) override;
    void send_command(const char* msg, int wait = SEND_WAIT_NONE) override;

    // 移动入口:成功移动后统一触发自动合并检查(堆叠物品专用)
    virtual int move(CContainer* me);
    // 自动合并检查(默认空实现;可堆叠物品由子类启用)
    virtual void check_auto_combine();
    virtual void init();
    static void destruct_me(CContainer* ob, int32_t param1, int32_t param2);

    int32_t receive_wound(const char* type, int32_t damage);
    int32_t receive_damage(const char* szType, int32_t damage);
    int32_t add_amount(int32_t val);
    virtual void set_amount(int32_t amount);
    virtual int32_t query_amount();
    virtual int32_t query_max_amount();
    CItem* split(int32_t amount);
    virtual int32_t query_attr(int attr);

    virtual int do_use(CChar* me, CContainer* target);
    virtual int can_use();

    CItem();
    virtual ~CItem();

protected:
    // 箱子内是否允许自动合并(默认允许;子类可关闭)
    virtual int can_combine_in_box() { return 1; }

protected:
    int32_t m_amount;
    int32_t m_max_amount;
};
