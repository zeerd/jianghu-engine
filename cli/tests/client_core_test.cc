// 构建/运行（仓库根）：.\run-coverage.ps1
#include "gtest/gtest.h"

#include <filesystem>
#include <memory>
#include <string>

#include "client_core.h"
#include "mock_connection.h"

namespace {

// 注入 MockConnection 的 ClientCore；不建连、不起网络线程。
struct CoreFixture {
    std::filesystem::path logs_dir;
    std::unique_ptr<MockConnection> mock;
    MockConnection *conn = nullptr;
    std::unique_ptr<ClientCore> core;

    explicit CoreFixture(int heartbeat_interval = 0)
    {
        logs_dir = std::filesystem::temp_directory_path()
                   / "jianghu_cli_client_core_test";
        std::filesystem::create_directories(logs_dir);
        Config cfg;
        cfg.logs_dir           = logs_dir.string();
        cfg.heartbeat_interval = heartbeat_interval;
        mock                   = std::make_unique<MockConnection>();
        conn                   = mock.get();
        core = std::make_unique<ClientCore>(cfg, std::move(mock));
    }

    ~CoreFixture()
    {
        std::error_code ec;
        std::filesystem::remove_all(logs_dir, ec);
    }
};

}  // namespace

// 保活载荷契约："<player_id> tune 0 reducedrop\t" —— 服务端对任何入站命令
// 都会在 CUser::process_command 里 do_active 重置发呆计时；载荷丢失或改坏
// 意味着挂机的 CLI 玩家会在发呆超时后被踢出。
TEST(ClientCoreHeartbeat, SendsTuneKeepAliveOnBeat)
{
    CoreFixture f;
    f.core->send_heartbeat();
    EXPECT_EQ(f.conn->last_command(), "0 tune 0 reducedrop\t");
}

// stop 后心跳循环必须立即退出且不再发包。若循环不检查 running_，
// wait_for 会等满 15 秒超时并补发一次保活，本断言即红。
TEST(ClientCoreHeartbeat, StoppedLoopExitsImmediatelyWithoutBeating)
{
    CoreFixture f;
    f.core->request_stop();
    f.core->run_heartbeat_loop(15);
    EXPECT_EQ(f.conn->command_count(), 0);
}

// 心跳线程生命周期：start_heartbeat 起线程，stop 唤醒并 join（不等满周期）。
// 挂死回归表现为本用例超时；若退化成 detach，线程会在 core 析构后访问
// 已释放成员而崩溃。
TEST(ClientCoreHeartbeat, StopWakesAndJoinsHeartbeatThread)
{
    CoreFixture f(15);
    f.core->start_heartbeat();
    EXPECT_TRUE(f.core->running());
    f.core->stop();
    EXPECT_FALSE(f.core->running());
}
