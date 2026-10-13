#include "CmdWizard.h"
#include "../npc/Npc.h"
#include "../room/FB.h"
#include "../room/Room.h"
#include "../item/Item.h"
#include "../item/FightRoom.h"

int CCmdWizard::where_me(CChar* me, int32_t target, char* arg)
{
    CContainer *env, *obj;
    char msg[255];

    if (!target) target = atol(arg);
    if (!(obj = find_object(target))) {
        message_vision_room(me, "tell", "【巫师】该物件不存在！");
        return 1;
    }

    if (!(env = obj->environment())) {
        snprintf(msg, 255, "【巫师】物件%s无环境。", obj->name());
        message_vision_room(me, "tell", msg);
        return 1;
    }

    snprintf(msg, 255, "【巫师】物件%s目前处于%s(%ld)中。", obj->name(),
             env->name(), env->object_id());
    message_vision_room(me, "tell", msg);
    return 1;
}

// 克隆NPC
int CCmdWizard::clone_npc(CChar* me, int32_t target, char* arg)
{
    CNpc* obj;
    char msg[255];

    if (!(obj = load_npc(arg))) return 0;  // 克隆失败

    obj->move(me->environment());

    tell_object(me,
                snprintf(msg, 255, "你凌空一指，变出一个%s来。", obj->name()));

    return 1;
}

// 克隆物品
int CCmdWizard::clone_object(CChar* me, int32_t target, char* arg)
{
    CContainer* obj;
    char msg[255], tmp[4];
    int32_t num;

    analyse_string(arg, msg, 40);
    num = atol(arg);
    strncpy(tmp, msg, 3);
    tmp[3] = 0;

    if (EQUALSTR(tmp, "zz_") && me->query("grant") < 100) return 0;

    if (!(obj = g_item_mgr->load_item(msg))) return 0;  // 克隆失败

    long id = obj->object_id();

    if (num) obj->set_amount(num);

    if (!obj->move(me))
        obj->move(load_room("pk"));  // 克隆移物品不成功，就移到pk去

    if (find_object(id)) {
        tell_object(
            me, snprintf(msg, 255, "你凌空一指，变出一个%s来。", obj->name(1)));
        LOG_INFO("Clone:%s(%s)凌空一指，变出一个%s来。", me->name(1), me->id(),
                 obj->name(1));
    }
    else
        return 0;

    return 1;
}

// 毁灭
int CCmdWizard::smash(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CContainer* who;

    if (!(who = get_room_object(me, target))
        && !(who = get_self_object(me, target)))
        return 0;

    snprintf(msg, 255, "%s从天上叫下一道闪电，劈在了%s身上。", me->name(),
             who->name());
    message_vision_room(me, "chat", msg);

    if (who->query_attr(CChar::IS_FIGHTROOM))  // 战斗
    {
        static_cast<CFightRoom*>(who)->end_fight();
        return 1;
    }

    // 生物
    if (who->is_character()) {
        static_cast<CChar*>(who)->die();
    }
    else if (!who->query_attr(CChar::IS_BOX)) {
        DESTRUCT_OB(who, "cmd::smash");
    }

    return 1;
}

// summon

void CCmdWizard::print_svalue(CChar* me, const char* key,
                              CMapping::svalue_t* sv)
{
    char msg[255];

    if (sv->type == T_NUMBER)
        tell_object(me, snprintf(msg, 255, "%s : %d", key, sv->u.number));
    else if (sv->type == T_STRING)
        tell_object(me, snprintf(msg, 255, "%s : %s", key, sv->u.string.get()));
    else if (sv->type == T_ARRAY) {
        tell_object(me, snprintf(msg, 255, "%s : ({", key));
        CVector* v = sv->u.arr.get();
        for (int i = 0; i < v->count(); i++)
            tell_object(me, snprintf(msg, 255, "    %s,", (*v)[i]));
        tell_object(me, "    })");
    }
    else if (sv->type == T_MAPPING) {
        tell_object(me, snprintf(msg, 255, "%s : ([", key));
        CMapping* m = sv->u.map.get();
        CVector v;

        m->keys(v);
        for (int i = 0; i < v.count(); i++) {
            CMapping::svalue_t* sv1 = m->find_string_in_mapping(v[i]);
            print_svalue(me, v[i], sv1);
        }

        tell_object(me, "    ])");
    }
}

// 列出物件数据
int CCmdWizard::show_data(CChar* me, CContainer* who)
{
    if (!who) return 0;
    char msg[255];
    CMapping* m = who->query_entire_dbase();
    tell_object(me, snprintf(msg, 255, "%s(%s):<%s>", who->name(1), who->id(1),
                             who->querystr("base_name")));

    CVector v;
    int i;

    m->keys(v);

    for (i = 0; i < v.count(); i++) {
        CMapping::svalue_t* sv = m->find_string_in_mapping(v[i]);
        print_svalue(me, v[i], sv);
    }

    m = who->query_entire_temp_dbase();

    tell_object(me, "临时数据:");
    v.clear();
    m->keys(v);

    for (i = 0; i < v.count(); i++) {
        CMapping::svalue_t* sv = m->find_string_in_mapping(v[i]);
        print_svalue(me, v[i], sv);
    }
    return 1;
}

int CCmdWizard::data(CChar* me, int32_t target, char* arg)
{
    CContainer* who;

    if (!target)
        who = me->environment();
    else if (!(who = get_room_object(me, target))
             && !(who = get_self_object(me, target))
             && !(who = find_player(target)))
        return notify_fail("请选中目标。");

    if (who && who->query_attr(CMainObject::IS_FBROOM)) {
        CFBRoom* fbr = static_cast<CFBRoom*>(who);
        if (fbr->m_parent_fb != NULL) {
            show_data(me, fbr->m_parent_fb);
        }
    }

    return show_data(me, who);
}
