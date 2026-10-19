-- 契约测试夹具：脚本执行期抛错（pcall 失败）

error("fixture runtime error")

return {
    update_condition = function(self, me, duration)
        return 0
    end,
}
