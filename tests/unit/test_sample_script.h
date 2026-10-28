// test_sample_script.h — 把 lua-sample 的样本脚本原文拷进单测脚本根的共享夹具
//
// 引擎只认 test_config.ini 的 script_root（+ default_script_root），而样本
// 脚本（lua-sample）不在其中；需要驱动真实样本脚本的用例用本夹具把原文暂存
// 进脚本根、用完即删——拷原文而不是复制内容，夹具不会与样本漂移。
// 引擎的 load_script 按内容 hash 缓存：文件出现/消失后下一次加载自动重试
// （见 LuaConditionTest.LoadFailure_RecoversWhenScriptFileAppears）。
//
// 用法：
//   ScopedSampleScript script("npc/monster_yongren.lua",
//                             "lua-default/npc/monster_yongren.lua");
//   ASSERT_TRUE(script.ok()) << "样本脚本未就位: " << script.file();
//
// Build+Run: .\run-coverage.ps1
#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "IniParser.h"

class ScopedSampleScript {
public:
    // sub_path：暂存到 script_root 下的相对路径；source：仓库内的样本脚本路径
    ScopedSampleScript(const char* sub_path, const char* source)
    {
        const std::string root = ini_get("lua", "script_root", "");
        if (root.empty()) return;
        m_file = std::filesystem::path(root) / sub_path;
        std::error_code ec;
        std::filesystem::create_directories(m_file.parent_path(), ec);

        std::ifstream in(source, std::ios::binary);
        if (!in) return;
        std::ofstream out(m_file, std::ios::binary);
        if (!out) return;
        out << in.rdbuf();
        m_ok = out.good();
    }

    ~ScopedSampleScript()
    {
        std::error_code ec;
        std::filesystem::remove(m_file, ec);
    }

    bool ok() const { return m_ok; }
    const std::filesystem::path& file() const { return m_file; }

    ScopedSampleScript(const ScopedSampleScript&)            = delete;
    ScopedSampleScript& operator=(const ScopedSampleScript&) = delete;

private:
    std::filesystem::path m_file;
    bool m_ok = false;
};
