
/**
 * @file GameSvr.h
 * @brief 游戏服务器网络层，管理网关连接、IOCP 收发与登录状态机。
 */

#pragma once
#include <list>
#include <mutex>
#include "ServerSockHandler.h"
#include "queue.h"

#define RUNGATE_STATUS_DISCONNECTED 0
#define RUNGATE_STATUS_GOOD         1
#define RUNGATE_STATUS_HEAVYTRAFFIC 2

#define LOGPARAM_STR 1
#define LOGPARAM_INT 2

#define MAX_ONLINE_USERS 5000

#define HAM_ALL      0
#define HAM_PEACE    1
#define HAM_GROUP    2
#define HAM_GUILD    3
#define HAM_PKATTACK 4

#define BAGGOLD 5000000

#define USERMODE_PLAYGAME 1
#define USERMODE_LOGIN    2
#define USERMODE_LOGOFF   3
#define USERMODE_NOTICE   4
#define BUFFER_SIZE       1024

// --- m_Login 连接登录状态机 ---
#define GATE_LOGIN_NOT_LOGGED_IN    0   // 未登录
#define GATE_LOGIN_LOGGING_IN       1   // 正在登录
#define GATE_LOGIN_NORMAL           2   // 正常已登录
#define GATE_LOGIN_CLOSING          -1  // 主动断开
#define GATE_LOGIN_PENDING_DELETE   -2  // 正在处理断开
#define GATE_LOGIN_SCHEDULED_DELETE -3  // 已加入删除队列

// --- _TRECVBUFF::type 接收消息类型 ---
#define RECV_MSG_LOGIN   0  // 登录消息
#define RECV_MSG_COMMAND 1  // 正常命令
#define RECV_MSG_EXIT    2  // 断开/退出

typedef struct tagOVERLAPPEDEX {
    OVERLAPPED Overlapped;
    INT nOvFlag;
    WSABUF DataBuf;
    CHAR Buffer[DATA_BUFSIZE * 2];
    int bufLen = 0;
} OVERLAPPEDEX, *LPOVERLAPPEDEX;

typedef struct tagOVERLAPPEDSEND {
    OVERLAPPED Overlapped;
    INT nOvFlag;
    WSABUF DataBuf;
    int bufLen = 0;
    class CGateInfo*
        gate;  // owning connection, used by send completion callback
} OVERLAPPEDSEND, *LPOVERLAPPEDSEND;

typedef struct tag_TSENDBUFF {
    int len;
    char szData[BUFFER_SIZE];
} _TSENDBUFF, *_LPTSENDBUFF;

typedef struct tag_TRECVBUFF {
    int len;
    int type;  // 0:登录   1:正常   2:断开
    char szData[BUFFER_SIZE];
} _TRECVBUFF, *_LPTRECVBUFF;

// send queue item: holds a heap buffer, avoids the 1024-byte limit of
// _TSENDBUFF
typedef struct tag_SENDITEM {
    int len;
    char* data;  // heap allocated, ownership transfers with the send
} _TSENDITEM, *_LPTSENDITEM;

/**
 * @brief 网关连接信息，管理单个客户端的网络状态与收发队列。
 */
class CGateInfo {
public:
    SOCKET m_sock;

    LONG m_do_sending;  // 0/1 atomic send channel (only one outstanding send at
                        // a time)
    CWHQueue m_send_buff_q;
    CWHQueue m_recv_buff_q;
    OVERLAPPEDEX OverlappedEx[2];
    int32_t m_player;  // 玩家OBJECTID，未登录时为0
    int32_t m_login;   // login state machine: 0 not logged in, 1 logging in, 2
                       // normal, -1..-3 closing/pending delete
    LONG m_ref_count;  // reference count: list + each outstanding IO
    bool m_closing;    // closing in progress

    std::recursive_mutex m_cs;  // protects m_Login/m_player/m_bClosing
                                // (recursive for reentrant calls)

public:
    CGateInfo();
    ~CGateInfo();

    void add_ref();
    void release();

    void x_send();
    // int		Send(_LPTSENDBUFF lpSendBuff);
    int send(const char* msg);
    int recv();
    int has_completion_packet();
    char* extract_packet(char* packet, int packetLen);

    // serialized send: pull data from queue and issue a single WSASend
    void try_send_next();
    // unified close path: set m_bClosing, push disconnect message, close socket
    void close_connection();
};
