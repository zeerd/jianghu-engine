-- 契约测试夹具：脚本返回表且含 init，但 init 运行期抛错

return {
    init = function(self, me)
        error("fixture init boom")
    end,
    update_condition = function(self, me, duration)
        return 0
    end,
}
