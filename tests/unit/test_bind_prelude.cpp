// test_bind_prelude.cpp — 启动期前导脚本（prelude）的装载、入参契约与可见性
//
// 构建/运行：.\run-coverage.ps1（从仓库根执行）
//
// 契约（src/lua/LuaBind.cpp lua_bind_run_prelude）：
//   - <script_root|default_script_root>/data/prelude.lua，前者优先；
//   - 以 jianghu_object 元表为唯一入参执行，脚本 local index = ... 取方法表；
//   - 全局环境定义的函数即全局 API；
//   - 文件缺席试下一根；文件在但读/载/执行失败记 ERROR 并停止。
// 探针根按进程隔离（多分片并行跑同一仓库时互不踩踏）。
#include "test_bind_common.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>

#include "IniParser.h"

class BindPreludeTest : public BindTest {
protected:
    std::filesystem::path m_root;  // 本进程专用的探针根

    void SetUp() override
    {
        ini_load("tests/unit/test_config.ini");
        BindTest::SetUp();  // 新状态 + 绑定注册（此时已装默认根前导）
        m_root = std::filesystem::path("test_tmp")
                 / ("prelude_probe_" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(m_root / "data");
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_root, ec);
        ini_load("tests/unit/test_config.ini");  // 复位本用例改过的配置
        BindTest::TearDown();
    }

    // 把配置的脚本根换成探针根，使下一次 lua_bind_init 装载探针脚本
    void UseProbeRoot()
    {
        WriteIni("probe.ini", m_root.string(), "lua-default");
    }

    void WriteIni(const char* name, const std::string& root0,
                  const std::string& root1)
    {
        const std::string path = (m_root / name).string();
        std::ofstream out(path, std::ios::trunc);
        out << "[lua]\n"
            << "script_root = " << root0 << "\n"
            << "default_script_root = " << root1 << "\n";
        out.close();
        ini_load(path.c_str());
    }

    void WriteProbe(const std::string& content)
    {
        std::ofstream out(m_root / "data" / "prelude.lua", std::ios::trunc);
        out << content;
    }

    // 前导执行失败后状态仍然可用：绑定照常注册
    // （探针取仍留在 C++ 绑定表的全局，不依赖前导定义）
    void ExpectBindingsIntact()
    {
        lua_getglobal(L, "notify_fail");
        EXPECT_TRUE(lua_isfunction(L, -1)) << "绑定表不受前导失败影响";
        lua_settop(L, 0);
    }
};

// 默认根的前导脚本必须以元表为入参可执行（入参契约）
TEST_F(BindPreludeTest, RealPreludeAcceptsMetatableArgument)
{
    const std::string path = "lua-default/data/prelude.lua";
    struct stat st;
    ASSERT_EQ(0, stat(path.c_str(), &st)) << "默认根必须提供前导脚本";
    const std::string gbk = load_file_as_gbk(path.c_str());
    ASSERT_FALSE(gbk.empty());
    ASSERT_EQ(LUA_OK,
              luaL_loadbuffer(L, gbk.c_str(), gbk.size(), path.c_str()));
    luaL_getmetatable(L, "jianghu_object");
    ASSERT_TRUE(lua_istable(L, -1));
    EXPECT_EQ(LUA_OK, lua_pcall(L, 1, 0, 0)) << lua_tostring(L, -1);
    lua_settop(L, 0);
}

// 探针前导：全局函数进 _G、index 上的方法可经对象调用
TEST_F(BindPreludeTest, ProbeGlobalsAndMethodsBecomeVisible)
{
    UseProbeRoot();
    WriteProbe(
        "local index = ...\n"
        "function prelude_probe_sum(a, b) return a + b end\n"
        "function index.probe_method(ob) return 42 end\n");
    lua_bind_init(L);  // 重新执行装载链（探针根命中）

    lua_getglobal(L, "prelude_probe_sum");
    ASSERT_TRUE(lua_isfunction(L, -1)) << "前导定义的全局函数可见";
    lua_pushinteger(L, 2);
    lua_pushinteger(L, 3);
    ASSERT_EQ(LUA_OK, lua_pcall(L, 2, 1, 0)) << lua_tostring(L, -1);
    EXPECT_EQ(5, lua_tointeger(L, -1));
    lua_settop(L, 0);

    CContainer box;
    box.set_name("probebox", "probe_box");
    lua_bind_push_object(L, &box);        // obj
    lua_getfield(L, -1, "probe_method");  // obj, fn（经 __index 解析）
    ASSERT_TRUE(lua_isfunction(L, -1)) << "index 上的方法可见";
    lua_pushvalue(L, -2);  // obj, fn, obj
    ASSERT_EQ(LUA_OK, lua_pcall(L, 1, 1, 0)) << lua_tostring(L, -1);
    EXPECT_EQ(42, lua_tointeger(L, -1)) << "ob:probe_method()";
    lua_settop(L, 0);
}

// 运行期错误：记 ERROR、状态可用、不中断装配
TEST_F(BindPreludeTest, ProbeRuntimeErrorIsIsolated)
{
    UseProbeRoot();
    WriteProbe("local index = ...\nerror('probe boom')\n");
    lua_bind_init(L);
    ExpectBindingsIntact();
}

// 语法错误：同上，load 失败即止
TEST_F(BindPreludeTest, ProbeSyntaxErrorIsIsolated)
{
    UseProbeRoot();
    WriteProbe("local index = ...\nthis is not lua(\n");
    lua_bind_init(L);
    ExpectBindingsIntact();
}

// 空文件：读入为空按读失败处理（记 ERROR，不回退默认根）
TEST_F(BindPreludeTest, ProbeEmptyFileIsReported)
{
    UseProbeRoot();
    WriteProbe("");
    lua_bind_init(L);
    ExpectBindingsIntact();
}

// 两根都无前导文件：记 ERROR（开机校验点），装配照常完成
TEST_F(BindPreludeTest, MissingInAllRootsIsReported)
{
    WriteIni("missing.ini", (m_root / "no_such_root").string(),
             (m_root / "no_such_default").string());
    lua_bind_init(L);
    ExpectBindingsIntact();
}
