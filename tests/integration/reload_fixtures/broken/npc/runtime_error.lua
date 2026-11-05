-- 故意的运行时错误变体：chunk 编译通过、执行即抛错——验证 pcall 失败
-- 分支同样保留旧版本。
error("e2e reload runtime boom")
return {}
