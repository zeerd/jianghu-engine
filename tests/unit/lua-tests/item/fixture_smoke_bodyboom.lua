-- 冒烟夹具：模块体执行期抛错（语法是好的，所以防线1 报不出来）。
-- 引擎 load_script 因此返回 NOREF，冒烟记成 load 违规。
error("smoke_body_boom")

return {}
