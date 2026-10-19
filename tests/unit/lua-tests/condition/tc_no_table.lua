-- 契约测试夹具：脚本必须返回 table，此处故意不返回（load_script 应失败）
function update_condition(self, me, duration)
    return CND_CONTINUE, duration
end
