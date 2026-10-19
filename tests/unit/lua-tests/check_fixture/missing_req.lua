-- require 闭包校验夹具：字面量 require 指向不存在的模块 -> 应失败
local missing = require("check_fixture.no_such_module")

return { missing = missing }
