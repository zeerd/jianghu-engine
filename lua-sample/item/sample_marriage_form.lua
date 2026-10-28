-- sample_marriage_form.lua  婚姻登记表
-- 示例物品：结婚登记的两段签名仪式。
-- 参考 jianghu-dataset/item/goods/tongxinyin.lua（同心印/同心结）的做法：
--   1. 第一方 use → 在**物品自己身上**记录签名者（sign/id、sign/name），
--      逾时未完成则签名淡去（call_out 清除，避免陈旧签名卡死流程）；
--   2. 持表的第二方 use → 校验签名者在同房间、双方未婚 → 双方建立
--      couple 映射，登记表焚毁。
-- 「和谁结婚」的状态载体是物品字段 + 房间在场（env:present），与参考一致。
-- 引擎没有 /marry 玩家命令，也不要在玩家或 NPC 身上发明 marry_target 之类的
-- 派生状态——第二方签名本身就是「指定对象」。

return {
    -- create(self, nFlag)
    create = function(self, nFlag)
        self:set_name("婚姻登记表", "hun yin deng ji biao")
        self:set("unit", "张")
        self:set("long", "一张印着鸳鸯戏水图的婚姻登记表，右下角留着两处空白的签名栏。")
    end,

    -- 声明可使用：use 命令据此放行（与 sample_hp 同形）
    Can_Use = function(self)
        return 1
    end,

    -- do_use(self, me, target)：target 未用到——签名双方由登记表自己记录
    do_use = function(self, me, target)
        if me:querymap("couple") then
            return notify_fail("你已是有家室的人了，还登记什么？")
        end

        local sign_id = self:querystr("sign/id")

        -- 第一段：自己签名，把签名者记在物品身上
        if sign_id == "" then
            self:set("sign/id", me:querystr("id"))
            self:set("sign/name", me:querystr("name"))
            message_vision("$N提起笔，在婚姻登记表上签下了自己的名字。", me)
            tell_object(
                me,
                "把登记表交给意中人，由对方在你身边签下第二个名字，登记才算完成。"
            )
            self:remove_call_out("fade_sign")
            self:call_out("fade_sign", 120)
            return 1
        end

        -- 已签过的人不能补第二签：结构上排除「自己娶自己」
        if sign_id == me:querystr("id") then
            return notify_fail("你已经签过名了，等对方来签吧。")
        end

        -- 第二段：签名者必须此刻就在同一房间
        local env = me:environment()
        local other = env and env:present(sign_id) or nil
        if not other then
            return notify_fail("签名之人不在这儿，登记表暂不作数。")
        end

        if other:querymap("couple") then
            return notify_fail("对方已有家室，登记被驳回。")
        end

        -- 建立婚姻关系（与月老离婚流程读写同一组 couple 字段）
        me:set("couple/name", other:querystr("name"))
        me:set("couple/id", other:querystr("id"))
        other:set("couple/name", me:querystr("name"))
        other:set("couple/id", me:querystr("id"))

        self:remove_call_out("fade_sign")
        message_vision("$N与$n在婚姻登记表上签下彼此的名字，结为夫妻。", me, other)
        destruct(self)
        return 1
    end,

    -- 逾时未签：签名淡去，登记表回到无人签名状态
    fade_sign = function(self)
        local env = self:environment()

        self:del("sign/id")
        self:del("sign/name")

        if is_user(env) then
            tell_object(env, "婚姻登记表上的签名渐渐淡去了……")
        elseif env then
            tell_room(env, "婚姻登记表上的签名渐渐淡去了……")
        end
    end,
}
