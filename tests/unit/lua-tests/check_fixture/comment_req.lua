-- require 闭包校验夹具：注释与字符串里的 require 示例不应被收集
-- require("check_fixture.not_exists_in_comment")
local example = "require(\"check_fixture.not_exists_in_string\")"

return { example = example }
