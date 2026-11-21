// test_bind_move.cpp — self:move(target) 返回值契约
//
// 构建与运行（仓库根）：.\run-unit-tests.ps1
// 聚焦：x64\Release\tests.exe --gtest_filter=BindMoveTest.*
//
// 契约：move 透传 CContainer::move 的结果——移入成功返回 true，失败返回
// false（自己移自己 / 承重超限 / 无目标）。脚本侧的成败分支全靠这个布尔值：
// `if not obj:move(me)`（化缘、乞讨、偷窃、寻宝、新手木剑）、
// `local r = me:move(x); if r then`（迷宫回程）等；绑定丢弃返回值时这些
// 分支恒真/恒假，按 nil 判支即走错。

#include "test_bind_common.h"

namespace {

class BindMoveTest : public BindTest {
protected:
    CContainer src;
    CContainer dst;

    void SetUp() override
    {
        BindTest::SetUp();
        src.set_name("src", "mv_src");
        dst.set_name("dst", "mv_dst");
        // 目标按真实房间口径置 infinity：跳过承重判定，成功支只考察 move 自身
        dst.set("infinity", 1);
    }

    // 调 l_move（target 为空即无目标调用）。返回值必须恰为一个 boolean，
    // 返回其真假；个数或类型不符记 ADD_FAILURE 并回 false。
    bool MoveRet(CContainer* target)
    {
        lua_bind_push_object(L, &src);
        if (target) lua_bind_push_object(L, target);

        const int k = call(l_move, target ? 2 : 1);
        if (k != 1) {
            ADD_FAILURE() << "move 应返回 1 个值，实际 " << k;
            if (k > 0) lua_pop(L, k);
            return false;
        }
        if (!lua_isboolean(L, -1)) {
            ADD_FAILURE() << "move 应返回 boolean，实际 "
                          << luaL_typename(L, -1);
            lua_pop(L, 1);
            return false;
        }
        const bool v = lua_toboolean(L, -1);
        lua_pop(L, 1);
        return v;
    }
};

TEST_F(BindMoveTest, MoveToContainer_ReturnsTrue)
{
    EXPECT_TRUE(MoveRet(&dst)) << "移入成功应返回 true";
}

TEST_F(BindMoveTest, MoveToSelf_ReturnsFalse)
{
    EXPECT_FALSE(MoveRet(&src))
        << "自己移自己（CContainer::move 返回 0）应返回 false";
}

TEST_F(BindMoveTest, MoveWithoutTarget_ReturnsFalse)
{
    EXPECT_FALSE(MoveRet(nullptr)) << "无目标不发生移动，应返回 false";
}

}  // namespace
