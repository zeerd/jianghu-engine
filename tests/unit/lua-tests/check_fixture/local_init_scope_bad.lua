-- 夹具：局部变量的作用域从声明语句**之后**开始——`local X = {...}` 构造器
-- 内部的 X 解析成全局（运行期为 nil），静态必须按未绑定全局报出来。
-- 数据集曾因 `return {` → `local X = {` 的改写在 141 个文件里引入这一类错误。
local probe = {
    boom = function(self)
        if probe == nil then
            error("probe reads the global inside its own initializer")
        end
    end,
}

return probe
