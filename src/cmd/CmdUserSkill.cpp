#include "CmdUser.h"
#include "../skill/Skill.h"
#include "../interfaces/IClientObject.h"

// skill
// 查看who的技能。
int CCmdUser::skill(CChar* me, int32_t target, char* arg)
{
    CChar* who = get_room_char(me, target);
    if (!who) who = me;

    // 判断me是否有权限看who的技能。
    if (!is_wizard(me) && me != who && !is_relative(me, who)
        && !who->recognize_apprentice(me))
        return notify_fail("你无权查看对方技能。");

    who->flush_my_skill(me);
    return 1;
}

// 下载技能
int CCmdUser::download_skill(CChar* me, int32_t target, char* arg)
{
    CSkill* skill;

    if (!(skill = load_skill(arg))) return 0;
    if (skill->query("selfcreate")) skill->reset(me);
    skill->download(me);
    return 1;
}

// 下载物件
int CCmdUser::download_object(CChar* me, int32_t target, char* arg)
{
    CContainer* obj;

    if ((obj = find_object(target)))
        dynamic_cast<IClientObject*>(obj)->download(me);
    return 1;
}

int CCmdUser::downloaduser(CChar* me, int32_t target, char* arg)
{
    CUser* who = find_player(target);
    if (!who) return 0;

    static_cast<CUser*>(me)->receive_user_info(who);
    return 1;
}
