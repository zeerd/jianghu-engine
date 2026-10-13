/**
 * @file ServerSockHandler.h
 * @brief IOCP 服务器套接字处理器，管理监听/连接/IOCP 线程池。
 */

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <vector>

#define DATA_BUFSIZE 8192

typedef enum {
    OVERLAPPED_RECV,
    OVERLAPPED_SEND,
} OVERLAPPED_FLAG;

typedef struct tag_TCOMPLETIONPORT {
    OVERLAPPED Overlapped;
    WSABUF DataBuf;
    CHAR Buffer[DATA_BUFSIZE];
    INT nOvFlag;
} _TCOMPLETIONPORT, *_LPTCOMPLETIONPORT;

typedef struct tag_TOVERLAPPEDEX {
    CHAR Buffer[DATA_BUFSIZE];
    WSABUF DataBuf;
    SOCKET Socket;
    WSAOVERLAPPED Overlapped;
    uint32_t BytesSEND = 0;
    uint32_t BytesRECV = 0;
    INT nOvFlag;
} _TOVERLAPPEDEX, *_LPTOVERLAPPEDEX;

// AcceptEx 预投递数量
#define ACCEPTEX_PENDING_COUNT 4

// 监听 socket 关联 IOCP 时使用的 completion key，用于区分 accept 完成与数据完成
#define IOCP_ACCEPT_KEY ((ULONG_PTR)0xFFFFFFFF)

// AcceptEx 重叠结构
typedef struct tag_ACCEPTOVERLAPPED {
    OVERLAPPED Overlapped;
    SOCKET AcceptSocket;
    CHAR Buffer[(sizeof(SOCKADDR_IN) + 16) * 2];
    uint32_t dwBytesReceived = 0;
} ACCEPTOVERLAPPED, *LPACCEPTOVERLAPPED;

bool init_server_socket(SOCKET& s, SOCKADDR_IN* addr, UINT nMsgID, int port,
                        long lEvent = 2);
bool clear_socket(SOCKET& s);

INT create_iocp_worker_thread(int thread);

bool init_thread(LPTHREAD_START_ROUTINE lpRoutine);

// 预投递一个 AcceptEx
bool post_accept_ex();
// 处理一个 accept 完成事件
void handle_accept_completion(LPACCEPTOVERLAPPED lpAcceptOv);

extern HANDLE g_iocp;

extern bool g_terminated;
extern SOCKET g_ssock;
extern SOCKADDR_IN g_saddr;
extern HANDLE g_event;

// Thread tracking for proper shutdown
extern std::vector<HANDLE> g_worker_threads;
void signal_iocp_threads_exit();
void wait_for_all_worker_threads(DWORD timeoutMs = 10000);
