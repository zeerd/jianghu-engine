// test_log_runid.cpp — Logger run id：每次测试运行写入独立日志文件
//
// 守护契约：
//   1. 日志文件名 = <path>/<月>-<日>[-<runid>].txt（月、日不补零）
//   2. 未设置 run id 时保持 <path>/<月>-<日>.txt（生产服务器按日分文件）
//   3. 真实写入落在带 run id 的文件上，且不再写入无 run id 的日期文件
//
// 现有覆盖为什么抓不住：test_log_sections.cpp 只断言内存中的 BEGIN/END 标记，
// 从不校验日志落盘的文件名；run id 拼接被丢弃时所有运行仍会追加进同一文件。
#include "pch.h"
#include <stdio.h>
#include <string>
#include <time.h>
#include <windows.h>
#include "Logger.h"

namespace {

// 契约样例日期：9 月 15 日 → "9-15"（月、日不补零）
static const int kMonth = 9;
static const int kDay   = 15;

// 本 fixture 独占一个临时目录，避免污染真实的测试日志目录
class LogRunIdTest : public ::testing::Test {
protected:
    std::string m_origPath;
    std::string m_origRunId;
    std::string m_tempDir;

    void SetUp() override
    {
        m_origPath  = logger_get_path();
        m_origRunId = logger_get_run_id();
        m_tempDir   = m_origPath + "/runid_test";

        logger_set_path(m_tempDir.c_str());
        logger_set_run_id("");
    }

    void TearDown() override
    {
        logger_set_run_id(m_origRunId.c_str());
        logger_set_path(m_origPath.c_str());

        WIN32_FIND_DATAA data;
        HANDLE h = FindFirstFileA((m_tempDir + "/*.txt").c_str(), &data);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                DeleteFileA((m_tempDir + '/' + data.cFileName).c_str());
            } while (FindNextFileA(h, &data));
            FindClose(h);
        }
        RemoveDirectoryA(m_tempDir.c_str());
    }

    // 今天作为 "月-日" 前缀（与日志写入时刻同一天）
    static std::string TodayPrefix()
    {
        time_t t       = time(NULL);
        struct tm* now = localtime(&t);
        return std::to_string(now->tm_mon + 1) + '-'
               + std::to_string(now->tm_mday);
    }

    static std::string ReadAll(const std::string& file)
    {
        FILE* fp = fopen(file.c_str(), "rb");
        if (!fp) return "";
        std::string content;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) content.append(buf, n);
        fclose(fp);
        return content;
    }
};

}  // namespace

// ====================================================================
// 文件名构造：默认（无 run id）保持按日分文件
// ====================================================================
TEST_F(LogRunIdTest, DefaultFileNameIsDateOnly)
{
    logger_set_run_id("");
    EXPECT_EQ(logger_log_file_name(kMonth, kDay), m_tempDir + "/9-15.txt");
}

// ====================================================================
// 文件名构造：run id 追加在日期之后
// ====================================================================
TEST_F(LogRunIdTest, RunIdAppendedAfterDate)
{
    logger_set_run_id("abc");
    EXPECT_EQ(logger_log_file_name(kMonth, kDay), m_tempDir + "/9-15-abc.txt");
    EXPECT_EQ(logger_get_run_id(), "abc");
}

// ====================================================================
// 复位：空串与 nullptr 都回到按日分文件
// ====================================================================
TEST_F(LogRunIdTest, EmptyOrNullRunIdRestoresDateFileName)
{
    logger_set_run_id("abc");

    logger_set_run_id("");
    EXPECT_EQ(logger_log_file_name(kMonth, kDay), m_tempDir + "/9-15.txt");

    logger_set_run_id("abc");
    logger_set_run_id(nullptr);
    EXPECT_EQ(logger_get_run_id(), "");
    EXPECT_EQ(logger_log_file_name(kMonth, kDay), m_tempDir + "/9-15.txt");
}

// ====================================================================
// 新 run id 非空且含进程 ID：同一秒并发运行也不会写进同一文件
// ====================================================================
TEST_F(LogRunIdTest, NewRunIdContainsProcessId)
{
    const std::string runId = logger_new_run_id();
    EXPECT_FALSE(runId.empty());
    EXPECT_NE(runId.find(std::to_string(GetCurrentProcessId())),
              std::string::npos);
}

// ====================================================================
// 真实写入：日志落在带 run id 的文件，且不产生无 run id 的日期文件
// ====================================================================
TEST_F(LogRunIdTest, LogMessageWritesRunScopedFileOnly)
{
    const std::string runId  = logger_new_run_id();
    const std::string marker = "runid marker " + runId;
    logger_set_run_id(runId.c_str());

    const std::string datePrefix = TodayPrefix();
    const std::string runFile
        = m_tempDir + '/' + datePrefix + '-' + runId + ".txt";
    const std::string dateFile = m_tempDir + '/' + datePrefix + ".txt";

    log_message(LOG_LVL_INFO, "%s", marker.c_str());

    ASSERT_NE(GetFileAttributesA(runFile.c_str()), INVALID_FILE_ATTRIBUTES)
        << "missing log file: " << runFile;
    EXPECT_EQ(GetFileAttributesA(dateFile.c_str()), INVALID_FILE_ATTRIBUTES)
        << "run id was ignored, wrote to shared file: " << dateFile;
    EXPECT_NE(ReadAll(runFile).find(marker), std::string::npos)
        << "marker not found in " << runFile;
}
