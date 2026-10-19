#pragma once

#include <string>
#include <vector>

#include "connection.h"

class MockConnection : public Connection {
public:
    std::vector<std::string> sent_commands;

    bool send_command_utf8(const std::string &cmd) override
    {
        sent_commands.push_back(cmd);
        return true;
    }

    bool send(const std::string &data) override
    {
        // send_command_utf8 calls send internally after encoding.
        // Mock records the raw bytes too.
        sent_raw_.push_back(data);
        return true;
    }

    std::string local_ip() const override { return "127.0.0.1"; }

    // Hide base drain_received to return injected messages (base method is now virtual).
    std::vector<std::string> drain_received() override
    {
        std::lock_guard<std::mutex> lock(recv_mutex_);
        auto out = recv_queue_;
        recv_queue_.clear();
        return out;
    }

    // Test helper: inject a raw message (will be returned by drain_received).
    void inject_received(const std::string &raw_message)
    {
        std::lock_guard<std::mutex> lock(recv_mutex_);
        recv_queue_.push_back(raw_message);
    }

    // Test helpers
    std::string last_command() const
    {
        return sent_commands.empty() ? "" : sent_commands.back();
    }

    std::string last_raw() const
    {
        return sent_raw_.empty() ? "" : sent_raw_.back();
    }

    void clear_commands()
    {
        sent_commands.clear();
        sent_raw_.clear();
    }

    int command_count() const { return static_cast<int>(sent_commands.size()); }
    const std::vector<std::string> &get_sent_commands() const
    {
        return sent_commands;
    }

private:
    std::vector<std::string> sent_raw_;
    std::mutex recv_mutex_;
    std::vector<std::string> recv_queue_;
};