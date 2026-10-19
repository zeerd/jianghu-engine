-- 契约测试夹具：update_condition 运行期抛错

return {
    update_condition = function(self, me, duration)
        error("boom")
    end,
}
