#include "connection.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

#include "protocol.h"

Connection::Connection() = default;

Connection::~Connection() { disconnect(); }

bool Connection::connect(const std::string &host, int port)
{
    if (!net_init()) {
        std::cerr << "[ERROR] network init failed\n";
        return false;
    }

    struct addrinfo hints {
    }, *res;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
        std::cerr << "[ERROR] DNS lookup failed for " << host << "\n";
        return false;
    }

    sock_fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_fd_ == kInvalidSocket) {
        freeaddrinfo(res);
        std::cerr << "[ERROR] socket: " << net_error_message() << "\n";
        return false;
    }

    if (::connect(sock_fd_, res->ai_addr,
                  static_cast<socket_len_t>(res->ai_addrlen))
        != 0) {
        freeaddrinfo(res);
        std::cerr << "[ERROR] connect: " << net_error_message() << "\n";
        close_socket(sock_fd_);
        sock_fd_ = kInvalidSocket;
        return false;
    }
    else {
        sockaddr_in name{};
        socket_len_t namelen = sizeof(name);
        if (getsockname(sock_fd_, reinterpret_cast<sockaddr *>(&name),
                        &namelen)
            == 0) {
            char buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &name.sin_addr, buffer, sizeof(buffer));
            local_ip_ = buffer;
        }
    }

    freeaddrinfo(res);

    // Set non-blocking for the connect itself to not block us
    set_nonblocking(sock_fd_, true);

    connected_ = true;
    running_   = true;

    recv_thread_ = std::thread(&Connection::receive_loop, this);
    return true;
}

bool Connection::is_connected() const
{
    return connected_ && sock_fd_ != kInvalidSocket;
}

std::string Connection::local_ip() const { return local_ip_; }

bool Connection::send(const std::string &data)
{
    if (!is_connected()) return false;

    std::string final = data + "\n";
    int len           = static_cast<int>(final.size());
    int written       = ::send(sock_fd_, final.data(), len, MSG_NOSIGNAL);
    return written == len;
}

/* Internal helper: UTF-8 → GBK → XOR → TCP, returns success. */
bool Connection::encode_and_send(const std::string &utf8_text)
{
    if (!is_connected()) return false;
    std::string gbk     = protocol::utf8_to_gbk(utf8_text);
    std::string encoded = protocol::encode_str(gbk);
    return send(encoded);
}

bool Connection::send_login(const std::string &account,
                            const std::string &password)
{
    if (!is_connected()) return false;
    std::string wire = protocol::format_login(local_ip_, "JJ.0.0.1", account,
                                              password, "", "");
    return encode_and_send(wire);
}

bool Connection::send_register(const std::string &account,
                               const std::string &password,
                               const std::string &gender, int strength,
                               int constitution, int intelligence,
                               int dexterity)
{
    if (!is_connected()) return false;
    std::string wire = protocol::format_register("JJ.0.0.1", account, password,
                                                 gender, strength, constitution,
                                                 intelligence, dexterity);
    return encode_and_send(wire);
}

bool Connection::send_command_utf8(const std::string &utf8_bytes)
{
    return encode_and_send(utf8_bytes);
}

void Connection::disconnect()
{
    running_   = false;
    connected_ = false;

    if (sock_fd_ != kInvalidSocket) {
        shutdown(sock_fd_, SHUT_RDWR);
        close_socket(sock_fd_);
        sock_fd_ = kInvalidSocket;
    }

    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}

std::vector<std::string> Connection::drain_received()
{
    std::lock_guard<std::mutex> lock(recv_mutex_);
    std::vector<std::string> result = std::move(recv_queue_);
    recv_queue_.clear();
    return result;
}

void Connection::receive_loop()
{
    char buf[4096];
    while (running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock_fd_, &fds);

        struct timeval tv {
            0, 100000
        };  // 100ms timeout
        int ret = select(static_cast<int>(sock_fd_) + 1, &fds, nullptr,
                         nullptr, &tv);
        if (ret < 0) {
            if (running_ && !net_interrupted())
                std::cerr << "[ERROR] select: " << net_error_message() << "\n";
            break;
        }
        if (ret == 0) continue;  // timeout, check running_

        int n = recv(sock_fd_, buf, static_cast<int>(sizeof(buf) - 1), 0);
        if (n <= 0) {
            if (n == 0 || !net_would_block()) {
                // Connection closed
                break;
            }
            continue;
        }

        buf[n]           = '\0';
        std::string data = incomplete_message_ + std::string(buf, n);
        incomplete_message_.clear();

        // Split on \t
        size_t pos;
        while ((pos = data.find('\t')) != std::string::npos) {
            std::string msg = data.substr(0, pos + 1);
            data.erase(0, pos + 1);

            if (!msg.empty()) {
                std::lock_guard<std::mutex> lock(recv_mutex_);
                recv_queue_.push_back(std::move(msg));
            }
        }
        incomplete_message_ = data;  // leftover partial message
    }

    connected_ = false;
    running_   = false;
}
