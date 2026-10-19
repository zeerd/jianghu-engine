/**
 * @file RegisterHall.cpp
 * @brief 注册大厅副本化的统一入口。
 */

#include "RegisterHall.h"

#include "Char.h"
#include "Container.h"
#include "FB.h"
#include "Global.h"
#include "ItemMgr.h"
#include "Logger.h"
#include "Room.h"
#include "RoomMgr.h"

// 游客登录与脚本拒绝准入（login.on_login 返回 2）都从这里进大厅。
// 每次调用造一个新的 rk_fb 副本实例，玩家互不挤同一房间；begin_fb 挂上
// 心跳后，实例空置即被 CFB 自动回收（房内有人时计时归零，不会误伤）。
void enter_register_hall(CChar* me)
{
    CFB* hall = dynamic_cast<CFB*>(g_item_mgr->load_fb(REGISTER_FB_NAME));

    if (hall && hall->move_in(me)) {
        hall->begin_fb(REGISTER_HALL_NAME);  // 回收出口 = 全局大厅
        return;
    }

    LOG_ERROR("enter_register_hall: 注册副本不可用，回落全局大厅");
    if (hall) hall->end_fb();  // 接不住人的半成品实例就地回收
    me->move(load_room(REGISTER_HALL_NAME));
}
