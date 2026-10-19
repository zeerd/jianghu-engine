-- require 闭包校验夹具：字面量 require 指向存在的模块 -> 应通过
local helper = require("check_fixture.helper")

return { ok = helper ~= nil }
