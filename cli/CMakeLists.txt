cmake_minimum_required(VERSION 3.16)
project(jianghu-cli
    VERSION 1.0
    DESCRIPTION "金剑江湖 CLI Client"
    LANGUAGES CXX
)

# Windows 构建已并入引擎解决方案（仓库根 jianghu.sln，项目 jianghu_cli）。
# 本文件仅用于 Linux/POSIX 的独立构建。
if(WIN32)
    message(FATAL_ERROR
        "Windows 构建请使用引擎解决方案 jianghu.sln（项目 jianghu_cli）。")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---- Coverage build ----
option(COVERAGE "Enable LLVM source-based code coverage" OFF)
if(COVERAGE)
    # Require Clang for coverage
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "COVERAGE=ON requires Clang. "
                            "Use -DCMAKE_CXX_COMPILER=clang++")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-instr-generate")
    message(STATUS "Coverage build enabled (LLVM source-based)")
endif()

# ---- Unit tests ----
option(BUILD_TESTS "Build unit tests" ON)

find_package(Threads REQUIRED)

# iconv 用于 POSIX 平台的 GBK <-> UTF-8 转码（Windows 走 Win32 API）。
find_package(Iconv REQUIRED)
add_compile_definitions(LIBICONV_PLUG)

# Sources
file(GLOB_RECURSE SOURCES "src/*.cc")
add_library(jianghu_lib STATIC ${SOURCES})
target_include_directories(jianghu_lib PUBLIC src)
target_link_libraries(jianghu_lib PUBLIC Threads::Threads Iconv::Iconv)

add_executable(jianghu src/main.cc)
target_include_directories(jianghu PRIVATE src)
target_link_libraries(jianghu PRIVATE jianghu_lib)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
