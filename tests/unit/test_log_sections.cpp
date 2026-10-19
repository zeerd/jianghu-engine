// test_log_sections.cpp — BEGIN/END section markers for every test case
//
// Writes a marker line into the server log file when each test starts and
// ends, so the log produced during a test run can be split into sections:
//   ========== BEGIN Suite.Name ==========
//   ... test output ...
//   ========== END Suite.Name PASS 12 ms ==========
//
// The listener is registered globally before main() using the same static
// pattern as the global environment in test_env.cpp. logger_set_path() runs
// in that environment's SetUp(), which happens before the first OnTestStart.
#include "pch.h"
#include <stdio.h>
#include <string>
#include <vector>
#include "Logger.h"

// ----------------------------------------------------------------
// Marker line formats
// ----------------------------------------------------------------

static std::string FormatSectionBegin(const char* suite, const char* name)
{
    return std::string("========== BEGIN ") + suite + "." + name
           + " ==========";
}

static std::string FormatSectionEnd(const char* suite, const char* name,
                                    bool passed, long long elapsedMs)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "========== END %s.%s %s %lld ms ==========", suite, name,
             passed ? "PASS" : "FAIL", elapsedMs);
    return buf;
}

// ----------------------------------------------------------------
// Marker sink — writes to the log file and records the line in memory
// ----------------------------------------------------------------

// Markers written during the run; asserted by the tests below.
static std::vector<std::string> g_sectionMarkers;

static void WriteSectionMarker(const std::string& line)
{
    log_message(LOG_LVL_INFO, "%s", line.c_str());
    g_sectionMarkers.push_back(line);
}

// ----------------------------------------------------------------
// TestSectionLogger — logs BEGIN/END for every test case
// ----------------------------------------------------------------

class TestSectionLogger : public ::testing::EmptyTestEventListener {
public:
    void OnTestStart(const ::testing::TestInfo& test_info) override
    {
        WriteSectionMarker(
            FormatSectionBegin(test_info.test_case_name(), test_info.name()));
    }

    void OnTestEnd(const ::testing::TestInfo& test_info) override
    {
        const long long elapsedMs
            = static_cast<long long>(test_info.result()->elapsed_time());
        WriteSectionMarker(
            FormatSectionEnd(test_info.test_case_name(), test_info.name(),
                             test_info.result()->Passed(), elapsedMs));
    }
};

// Register before main; gtest takes ownership and deletes it at exit.
static struct TestSectionLoggerRegistrar {
    TestSectionLoggerRegistrar()
    {
        ::testing::UnitTest::GetInstance()->listeners().Append(
            new TestSectionLogger);
    }
} g_testSectionLogger;

// ====================================================================
// Test: the listener records a BEGIN marker for the running test
// ====================================================================
TEST(LogSectionTest, CurrentTestBeginRecorded)
{
    const ::testing::TestInfo* ti
        = ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string expected
        = FormatSectionBegin(ti->test_case_name(), ti->name());

    bool found = false;
    for (const std::string& marker : g_sectionMarkers) {
        if (marker == expected) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "missing marker: " << expected;
}

// ====================================================================
// Test: BEGIN marker format
// ====================================================================
TEST(LogSectionTest, BeginFormat)
{
    EXPECT_EQ(FormatSectionBegin("SmokeTest", "CreateContainer"),
              "========== BEGIN SmokeTest.CreateContainer ==========");
}

// ====================================================================
// Test: END marker format — pass and fail branches
// ====================================================================
TEST(LogSectionTest, EndFormatPass)
{
    EXPECT_EQ(FormatSectionEnd("SmokeTest", "CreateContainer", true, 12),
              "========== END SmokeTest.CreateContainer PASS 12 ms ==========");
}

TEST(LogSectionTest, EndFormatFail)
{
    EXPECT_EQ(FormatSectionEnd("SmokeTest", "CreateContainer", false, 3),
              "========== END SmokeTest.CreateContainer FAIL 3 ms ==========");
}
