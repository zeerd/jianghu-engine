/**
 * @file Corpse.cpp
 * @brief implementation of the CCorpse class. */

#include <windows.h>

#include "Corpse.h"
#include "Item.h"
#include "../interfaces/IHittable.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------

CCorpse::CCorpse()
{
    set_name("无名尸体", "corps");
    m_decayed = 0;
    m_delay   = 120;  // 120秒后腐烂
    set_heart_beat(1);
}

CCorpse::~CCorpse() {}

int32_t CCorpse::query_attr(int attr)
{
    switch (attr) {
        case IS_LIVING:
            return 0;
        case IS_CORPSE:
            return 1;
        case IS_CHAR:
            return 0;
        case CAN_GET:
            return 1;
    }

    return CChar::query_attr(attr);
}

// 腐烂
void CCorpse::decay()
{
    char msg[255];
    m_decayed++;
    switch (m_decayed) {
        case 1:
            snprintf(msg, 255, "%s开始腐烂了，发出一股难闻的恶臭。", name());
            message_vision_room(this, "chat", msg);
            set_name("腐烂的尸体");
            set("base_name", "corpse_ob");
            m_delay = 120;
            update_me();  // 更新
            break;
        case 2:
            snprintf(msg, 255, "%s被风吹干了，变成一具骸骨。", name());
            message_vision_room(this, "chat", msg);
            set_name("一具枯干的骸骨");
            m_delay = 60;
            update_me();  // 更新
            break;
        case 3:
            snprintf(msg, 255, "一阵风吹过，把%s化成骨灰吹散了。", name());
            message_vision_room(this, "chat", msg);
            CContainer* obj;
            CContainer* env = environment();
            DTItemList search;

            copy_list(&search, &m_item_list);
            for (auto val : search) {
                obj = val;
                obj->move(env);
            }
            DESTRUCT_OB(this, "CCorpse::Decay");
            break;
    }
}

void CCorpse::heart_beat()
{
    if (m_delay--) return;
    decay();
}

// 制造尸体
CCorpse* CCorpse::make_corpse(CChar* victim, CChar* killer)
{
    char msg[255];
    CCorpse* corpse;

    corpse = (CCorpse*)g_item_mgr->load_item("corpse_ob");

    snprintf(msg, 255, "%s的尸体", victim->name(1));
    corpse->set_name(msg, "corpse");

    if (is_user(victim) || victim->query("is_user")) corpse->set("is_user", 1);

    corpse->set("victim_name", victim->name(1));
    corpse->set("age", victim->query("age"));
    corpse->set("gender", victim->query("gender"));
    corpse->set_weight(victim->query_weight());
    corpse->set_max_encumbrance(victim->query_max_encumbrance());
    corpse->move(victim->environment());

    // Don't let wizard left illegal items in their corpses.
    if (!is_wizard(victim)) {
        DTItemList search;
        CContainer* obj;
        char cmd[255];

        copy_list(&search, victim->get_item_list());
        for (auto val : search) {
            obj = val;

            int32_t object_id = obj->object_id();
            if (obj->query("index"))
                victim->command(
                    snprintf(cmd, 255, "unwield %ld", obj->object_id()));
            dynamic_cast<IHittable*>(obj)->owner_is_killed(killer);  // 销毁

            // 检查obj是否已经被销毁
            if (object_id == g_last_destruct_object) continue;

            // 检查obj是否是owner的。
            if (DIFFERSTR(obj->querystr("owner"), victim->querystr("id")))
                obj->move(victim->environment());
        }

        search.clear();
    }

    return corpse;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// MakeCorpse(victim, killer) -> corpse|nil - 制造尸体（CCorpse::MakeCorpse）
int l_make_corpse(lua_State* L)
{
    CChar* victim = NULL;
    CChar* killer = NULL;
    if (lua_isuserdata(L, 1)) victim = dynamic_cast<CChar*>(get_object(L, 1));
    if (lua_isuserdata(L, 2)) killer = dynamic_cast<CChar*>(get_object(L, 2));
    if (!victim) {
        lua_pushnil(L);
        return 1;
    }
    CCorpse* corpse = CCorpse::make_corpse(victim, killer);
    lua_bind_push_object(L, corpse);
    return 1;
}
