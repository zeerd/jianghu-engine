-- require 闭包校验夹具：长括号形态
-- 长注释里的 require 示例不应被收集
--[[ require("check_fixture.not_exists_longcomment") ]]
-- 长字符串里的 require 示例不应被收集
local example = [[require("check_fixture.not_exists_longstring")]]
-- 长括号字面量模块名应被收集并成功解析
local helper = require([[check_fixture.helper]])

return { h = helper, e = example }
