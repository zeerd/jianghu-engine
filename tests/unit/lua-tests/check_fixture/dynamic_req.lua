-- require 闭包校验夹具：非字面量 require 无法静态校验 -> 应跳过且通过
local modname = "check_fixture.helper"
local helper = require(modname)

return { ok = helper ~= nil }
