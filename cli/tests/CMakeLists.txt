enable_testing()

file(GLOB_RECURSE TEST_SOURCES "*.cc")
add_executable(test_cases ${TEST_SOURCES})
target_include_directories(test_cases PRIVATE ../src)
target_link_libraries(test_cases PRIVATE jianghu_lib)

# POSIX 平台使用系统安装的 GTest（如 libgtest-dev）。
# Windows 构建走引擎解决方案，由 NuGet 引入 GTest。
find_package(GTest REQUIRED)
target_link_libraries(test_cases PRIVATE
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME test_cases COMMAND test_cases)
