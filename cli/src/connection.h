#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "compat.h"

class Connection {
public:
    Connection();
    virtual ~Connection();

    // Connect to server. Returns true on success.
    bool connect(const std::string &host, int port);

    bool is_connected() const;
    virtual std::string local_ip() const;

    /** Returns true if the server closed the connection (recv returned EOF). */
    bool is_eof() const { return !connected_ && sock_fd_ != kInvalidSocket; }

    /** Low-level: send pre-encoded bytes (appends "\n", returns success). */
    virtual bool send(const std::string &data);

    /** High-level protocol helpers — all handle UTF-8→GBK + XOR internally.
     *  Wire-format functions in protocol namespace are available for callers
     * who need to build custom strings. */
    bool send_login(const std::string &account, const std::string &password);
    bool send_register(const std::string &account, const std::string &password,
                       const std::string &gender, int strength,
                       int constitution, int intelligence, int dexterity);
    /** Generic: UTF-8 → GBK → XOR → TCP. Caller must build wire format string.
     */
    virtual bool send_command_utf8(const std::string &utf8_bytes);
    // Disconnect
    void disconnect();

    // Instead of a raw callback, we queue received messages into a buffer
    // that can be polled from the main thread
    virtual std::vector<std::string> drain_received();

private:
    void receive_loop();

    socket_t sock_fd_ = kInvalidSocket;
    std::string local_ip_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread recv_thread_;

    std::mutex recv_mutex_;
    std::vector<std::string> recv_queue_;
    std::string incomplete_message_;

    bool encode_and_send(const std::string &utf8_text);
};
