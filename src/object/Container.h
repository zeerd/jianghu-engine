/**
 * @file Container.h
 * @brief 容器类，游戏对象的核心基类，管理重量、容量、层级、查找与移动。
 *
 * CContainer 只负责「容器」职责：重量/容量管理、物件列表、层级移动、
 * 心跳/call_out、持久化。所有非容器行为（战斗、装备、动作分派、对话、
 * 客户端序列化）通过接口类（IHittable/IEquippable/IActionHandler/
 * IDialogSource/IClientObject）由子类各自实现。
 */

#pragma once

#include "CObject.h"
#include "Global.h"

class CChar;

typedef void (*CALLOUTFUNC)(CContainer* me, int32_t param1, int32_t param2);
typedef std::list<CContainer*> DTItemList;
void copy_list(DTItemList* dest, DTItemList* src);

/**
 * @brief 容器类，游戏世界中所有可包含其他对象的实体的基类。
 */
class CContainer : public CMainObject {
public:
    // ========================================================================
    //  构造 / 析构 / 创建
    // ========================================================================
    CContainer();
    virtual ~CContainer();
    virtual void create(int flag = 0);
    virtual void setup();
    virtual void destructed();

    // ========================================================================
    //  标识与属性查询
    // ========================================================================
    int32_t object_id();
    const char* title(int raw = 0);
    const char* short_desc(int raw = 0);
    const char* long_desc(int raw = 0);
    virtual const char* query_short();
    virtual int32_t query_attr(int attr);
    int is_character();
    int can_equipped() { return query_attr(CAN_EQUIPPED); }
    int is_room()
    {
        return query_attr(IS_ROOM) || query_attr(IS_FBROOM)
               || query_attr(IS_FB);
    }
    int is_box() { return query_attr(IS_BOX); }

    // ========================================================================
    //  重量与容量
    // ========================================================================
    int32_t weight();
    int32_t query_weight();
    void set_weight(int32_t weight);
    int32_t query_encumb();
    void add_encumbrance(int32_t w);
    int over_encumbranced();
    void over_encumbrance();
    int32_t query_max_encumbrance();
    void set_max_encumbrance(int32_t e);

    // ========================================================================
    //  容器层级 / 移动
    // ========================================================================
    CContainer* environment();
    virtual int move(CContainer* me);
    virtual void change_room(CContainer* me);
    virtual void on_moved(CContainer* me);
    virtual void on_leave(CContainer* me);
    virtual void on_enter(CContainer* me);
    void on_rem_object(CContainer* obj, CContainer* env);
    void on_add_object(CContainer* obj, CContainer* env);
    DTItemList* get_item_list();

    // ========================================================================
    //  查找
    // ========================================================================
    CContainer* present(const char* id, int flag = 0);
    CContainer* present(int32_t obj, int flag = 0);
    CContainer* present_name(const char* base_name, int type, int flag = 0);
    MAP2USER* get_users(const char* channel, MAP2USER* list);

    // ========================================================================
    //  数量与叠加
    // ========================================================================
    virtual void set_amount(int32_t amount);
    virtual int32_t add_amount(int32_t amount);
    virtual int32_t query_amount();
    virtual int32_t query_max_amount();

    // ========================================================================
    //  持久化
    // ========================================================================
    virtual int load();
    virtual int save();
    virtual int query_autoload();

    // 持久化路由（表名 + 对象 id）：脚本经 dbase 的 save_table/save_key 声明
    // 专属存盘表与该表 obj_id 的取值键；未声明则按对象类型默认
    // （t_room/t_npc/t_item + base_name）。C++ 不认识任何具体表名。
    void save_route(std::string& out_table, std::string& out_id);

    // ========================================================================
    //  心跳与 call_out
    // ========================================================================
    virtual void heart_beat();
    void set_heart_beat(int flag);
    virtual void evaluate_call_out_combat();
    void call_out_combat(CALLOUTFUNC func, time_t delay, int32_t param1 = 0,
                         int32_t param2 = 0);
    void call_out(CALLOUTFUNC func, time_t delay, int32_t param1 = 0,
                  int32_t param2 = 0);
    void remove_call_out(CALLOUTFUNC func, int evalute = 0);
    void remove_call_out_by_id(int32_t calloutId);

    // ========================================================================
    //  数据库
    // ========================================================================
    CMapping* query_entire_dbase();
    CMapping* query_entire_temp_dbase();

    // ========================================================================
    //  锁定 / 数据保护
    // ========================================================================
    int lock_data(int locked);

    // ========================================================================
    //  初始化与清理
    // ========================================================================
    virtual void init(CChar* me);
    virtual int purge();
    void remove();
    int visible();
    void update_me();
    void clean_up();

    // ========================================================================
    //  析构标记（外部管理器使用）
    // ========================================================================
    void set_destructed() { m_destruct = true; }
    bool is_destructed() const { return m_destruct; }

protected:
    // ========================================================================
    //  内部类型
    // ========================================================================
    typedef struct _CALLOUTNODE {
        CALLOUTFUNC func = nullptr;
        int type         = 0;
        time_t delay     = 0;
        int32_t param1   = 0;
        int32_t param2   = 0;
        // 回调执行期间为 true：节点仍被心跳的迭代器指着，移除路径必须跳过，
        // 回调结束后由心跳回收。
        bool executing = false;
    } CALLOUTNODE;

    // ========================================================================
    //  持久化辅助（供子类调用）
    // ========================================================================
    int save_impl(CMapping* savedata);
    int load_impl(CMapping* savedata);
    int lock_data_impl(int lock);

    // ========================================================================
    //  标识
    // ========================================================================
    int32_t m_object_id;

    // ========================================================================
    //  重量与容量
    // ========================================================================
    int32_t m_weight;
    int32_t max_encumb;
    int32_t encumb;

    // ========================================================================
    //  容器层级
    // ========================================================================
    DTItemList m_item_list;
    CContainer* m_environment;

    // ========================================================================
    //  状态标记
    // ========================================================================
    int32_t m_update;
    bool m_destruct;

    // ========================================================================
    //  call_out
    // ========================================================================
    std::list<std::unique_ptr<CALLOUTNODE>> m_callout_list;
};
