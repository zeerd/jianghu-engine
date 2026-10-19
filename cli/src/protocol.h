#pragma once

#include <string>
#include <vector>

#include "types.h"

namespace protocol {

// Parse a raw TCP segment into a GameMessage.
// Input: "&C=19&M=Hello&\t"
// Returns std::nullopt if parse fails.
std::optional<GameMessage> parse_message(const std::string &raw);

// Format a C-S command for sending.
// e.g. command("look")    -> "look\t"
// e.g. command("go", {"east"}) -> "go east\t"
// e.g. command("tell", {"123", "hello there"}) -> "tell 123 你好\t"
// e.g. immediate("save") -> "0 save\t"
std::string command(const std::string &cmd,
                    const std::vector<std::string> &args = {});
std::string immediate(const std::string &cmd,
                      const std::vector<std::string> &args = {});

// Format login packet
// version_prefix should be English for GBK
std::string format_login(const std::string &local_ip,
                         const std::string &version_prefix,
                         const std::string &account,
                         const std::string &password,
                         const std::string &card     = "",
                         const std::string &cardpass = "");

// For register
std::string format_register(const std::string &version_prefix,
                            const std::string &account,
                            const std::string &password,
                            const std::string &gender, int strength,
                            int constitution, int intelligence, int dexterity);

// Format tell packet (私聊) for sending private messages to a specific player
// Returns wire format: "{player_uid} tell {target_id} <content>\t"
std::string format_tell(int64_t uid, int64_t target_id,
                        const std::string &content);

// Format xchat packet (闲聊) for public chat broadcast to room
// Returns wire format: "{player_uid} chat 0 <content>\t"
std::string format_chat(int64_t uid, const std::string &content);

// XOR encode/decode (same function — XOR is symmetric)
// The server uses DecodeStr with key "\15L\26A\18N\12W\25O\33O\6D"
std::string encode_str(const std::string &input);

// XOR decode (symmetric — same as encode_str, but named for clarity on the
// receive path)
inline std::string decode_str(const std::string &input)
{
    return encode_str(input);
}

// Convert GBK to UTF-8 (server sends GBK encoded text)
std::string gbk_to_utf8(const std::string &input);

// Convert UTF-8 to GBK (server expects GBK encoded text)
std::string utf8_to_gbk(const std::string &input);

}  // namespace protocol
