-- 冒烟夹具：create 直接抛错。引擎侧 lua_pcall 会吞掉它、只写一行日志，
-- 冒烟必须把现场记成违规，否则这类错误只能等运行期撞见。
return {
    type = "item",
    create = function(self, flag)
        error("smoke_boom")
    end,
}
