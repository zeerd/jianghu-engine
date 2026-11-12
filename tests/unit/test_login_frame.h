// test_login_frame.h — on_login 网关帧构造（镜像 decode_str 的 XOR 编码器）
//
// 帧布局与 dispatch_gate_recv 压入的登录消息一致：[0..254] 为 XOR 编码的
// 登录串，[255..] 为对端 IP 串。BuildLoginFrame 内做 decode_str 还原自检，
// 明文 XOR 后撞上保留字节（空格/制表符/换行/NUL）返回 false——调用方须
// ASSERT_TRUE 并换用例参数。
//
// Build+Run: .\run-coverage.ps1
#pragma once

#include <cstring>

#include "StringUtil.h"  // decode_str（帧编码与之互逆）

// 明文格式：login <本地IP> <版本> <帐号> <密码> <卡号> <卡密>
inline const char* kLoginCmd = "login 8.8.8.8 1.0 10001 test1234 c1 p1";
// get_ip 的拼装结果：M:<对端IP> L:<本地IP>（各截 19 字符）
inline const char* kIpPair = "M:9.9.9.9 L:8.8.8.8";

// 镜像 decode_str 的 XOR 编码器：空格/换行/制表符原样保留且同样推进
// 密钥游标。
inline void encode_str(char* msg)
{
    int p = 0, t = 0;
    const char* key = "\15L\26A\18N\12W\25O\33O\6D";
    int len         = (int)strlen(key);
    while (msg[p]) {
        if (msg[p] != ' ' && msg[p] != '\n' && msg[p] != '\t') {
            msg[p] ^= key[t];
        }
        p++;
        t++;
        if (t >= len) t = 0;
    }
}

// 构造 on_login 帧：登录串编码进 [0..254]，IP 串放 [255..]。
// 返回 false 表示编码碰撞，需换用例参数——调用方须 ASSERT_TRUE。
inline bool BuildLoginFrame(char* buf, size_t cap, const char* plaintext,
                            const char* ip)
{
    memset(buf, 0, cap);
    if (strlen(plaintext) >= 254) return false;

    strcpy(buf, plaintext);
    char check[256];
    strcpy(check, buf);
    encode_str(check);
    decode_str(check);
    if (strcmp(check, plaintext) != 0) return false;  // 编码碰撞

    encode_str(buf);
    strncpy(&buf[255], ip, 79);
    return true;
}
