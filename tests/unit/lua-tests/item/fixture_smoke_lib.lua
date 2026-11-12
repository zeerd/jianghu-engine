-- 冒烟夹具：被 require 的 item 目录模块（库）。库没有对象生命周期，
-- 冒烟它只会得到"没有 create"的假结论，因此按点名跳过。
return {
    create = function(self, flag)
        error("lib_boom_should_not_run")
    end,
}
