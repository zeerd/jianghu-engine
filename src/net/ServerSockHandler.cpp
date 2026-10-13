#include <thread>
#include <vector>
#include "GameSvr.h"
#include "Global.h"

HANDLE g_iocp = NULL;

DWORD WINAPI server_worker_thread(LPVOID lpParameter);

INT create_iocp_worker_thread(int thread);

// Thread tracking for proper shutdown
std::vector<HANDLE> g_worker_threads;
std::mutex g_worker_threads_mutex;

// AcceptEx 函数指针（静态缓存，经 WSAIoctl 获取）
static LPFN_ACCEPTEX g_accept_ex                           = NULL;
static LPFN_GETACCEPTEXSOCKADDRS g_get_accept_ex_sockaddrs = NULL;

// 获取 AcceptEx 相关函数指针
static bool init_accept_ex_pointers()
{
    if (g_accept_ex && g_get_accept_ex_sockaddrs) return true;

    if (g_ssock == INVALID_SOCKET) return false;

    GUID GuidAcceptEx             = WSAID_ACCEPTEX;
    GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
    DWORD dwBytes                 = 0;

    if (WSAIoctl(g_ssock, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx,
                 sizeof(GuidAcceptEx), &g_accept_ex, sizeof(g_accept_ex),
                 &dwBytes, NULL, NULL)
        == SOCKET_ERROR)
        return false;

    if (WSAIoctl(g_ssock, SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
                 &g_get_accept_ex_sockaddrs, sizeof(g_get_accept_ex_sockaddrs),
                 &dwBytes, NULL, NULL)
        == SOCKET_ERROR)
        return false;

    return true;
}

// 预投递一个 AcceptEx
bool post_accept_ex()
{
    if (!init_accept_ex_pointers()) return false;

    LPACCEPTOVERLAPPED lpAcceptOv = new ACCEPTOVERLAPPED;
    if (!lpAcceptOv) return false;

    ZeroMemory(&lpAcceptOv->Overlapped, sizeof(OVERLAPPED));
    lpAcceptOv->AcceptSocket
        = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (lpAcceptOv->AcceptSocket == INVALID_SOCKET) {
        delete lpAcceptOv;
        return false;
    }

    DWORD dwBytes = 0;
    bool ret
        = g_accept_ex(g_ssock, lpAcceptOv->AcceptSocket, lpAcceptOv->Buffer,
                      0,                         // 不接收初始数据
                      sizeof(SOCKADDR_IN) + 16,  // 本地地址缓冲
                      sizeof(SOCKADDR_IN) + 16,  // 远端地址缓冲
                      &dwBytes, &lpAcceptOv->Overlapped);

    if (ret == false && WSAGetLastError() != ERROR_IO_PENDING) {
        closesocket(lpAcceptOv->AcceptSocket);
        delete lpAcceptOv;
        return false;
    }

    return true;
}

bool init_thread(LPTHREAD_START_ROUTINE lpRoutine)
{
    try {
        std::thread t(lpRoutine, nullptr);
        HANDLE hThread = t.native_handle();

        // Duplicate the handle to get our own reference that remains valid
        // after detach
        HANDLE hDupThread = NULL;
        if (!DuplicateHandle(GetCurrentProcess(), hThread, GetCurrentProcess(),
                             &hDupThread, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            t.detach();
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(g_worker_threads_mutex);
            g_worker_threads.push_back(hDupThread);
        }
        t.detach();
        return true;
    } catch (...) {
        return false;
    }
}

// ****************************************************************************
//
//
//
// ****************************************************************************

bool init_server_socket(SOCKET& s, SOCKADDR_IN* addr, UINT /*nMsgID*/, int port,
                        long /*lEvent*/)
{
    if (s == INVALID_SOCKET) {
        s = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

        addr->sin_family      = AF_INET;
        addr->sin_port        = htons(port);
        addr->sin_addr.s_addr = htonl(INADDR_ANY);

        if ((bind(s, reinterpret_cast<const struct sockaddr*>(addr),
                  sizeof(SOCKADDR_IN)))
            == SOCKET_ERROR)
            return false;

        if ((listen(s, 5)) == SOCKET_ERROR) return false;

        create_iocp_worker_thread((int)1);

        // 将监听 socket 关联到 IOCP，completion key 用 IOCP_ACCEPT_KEY 区分
        // accept 完成
        if (CreateIoCompletionPort((HANDLE)s, g_iocp, IOCP_ACCEPT_KEY, 0)
            == NULL)
            return false;

        // 预投递多个 AcceptEx
        for (int i = 0; i < ACCEPTEX_PENDING_COUNT; i++) {
            if (!post_accept_ex()) return false;
        }
    }
    else
        return false;

    return true;
}

bool clear_socket(SOCKET& s)
{
    if (s != INVALID_SOCKET) {
        LINGER lingerStruct;

        lingerStruct.l_onoff  = 1;
        lingerStruct.l_linger = 0;

        setsockopt(s, SOL_SOCKET, SO_LINGER, (char*)&lingerStruct,
                   sizeof(lingerStruct));

        closesocket(s);
        s = INVALID_SOCKET;

        return true;
    }

    return false;
}

INT create_iocp_worker_thread(int thread)
{
    // 仅在 IOCP 尚未创建时初始化（g_hIOCP 初始为 NULL）
    if (g_iocp == NULL) {
        SYSTEM_INFO SystemInfo;

        if ((g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0))
            == NULL)
            return -1;

        GetSystemInfo(&SystemInfo);

        for (UINT i = 0; i < SystemInfo.dwNumberOfProcessors * thread; i++) {
            try {
                std::thread t(server_worker_thread, g_iocp);
                HANDLE hThread = t.native_handle();

                // Duplicate the handle to get our own reference that remains
                // valid after detach
                HANDLE hDupThread = NULL;
                if (!DuplicateHandle(GetCurrentProcess(), hThread,
                                     GetCurrentProcess(), &hDupThread, 0, FALSE,
                                     DUPLICATE_SAME_ACCESS)) {
                    t.detach();
                    return -1;
                }

                {
                    std::lock_guard<std::mutex> lock(g_worker_threads_mutex);
                    g_worker_threads.push_back(hDupThread);
                }
                t.detach();
            } catch (...) {
                return -1;
            }
        }

        return SystemInfo.dwNumberOfProcessors * thread;
    }

    return -1;
}

// ****************************************************************************
//
//			处理一个 accept 完成事件
//
// ****************************************************************************

// g_xGateList 定义于 ServerSockWorker.cpp
extern std::list<CGateInfo*> g_gate_list;

void handle_accept_completion(LPACCEPTOVERLAPPED lpAcceptOv)
{
    if (!lpAcceptOv) return;

    // 使 AcceptSocket 继承监听 socket 的属性
    setsockopt(lpAcceptOv->AcceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
               (char*)&g_ssock, sizeof(g_ssock));

    // 创建客户
    CGateInfo* gate_info = new CGateInfo;

    if (gate_info) {
        gate_info->m_sock = lpAcceptOv->AcceptSocket;

        CreateIoCompletionPort((HANDLE)gate_info->m_sock, g_iocp,
                               (uint32_t)gate_info, 0);

        g_gate_list.push_back(gate_info);

        {
            int zero = 0;

            setsockopt(gate_info->m_sock, SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<char*>(&zero), sizeof(zero));

            ZeroMemory(&(gate_info->OverlappedEx), sizeof(OVERLAPPED) * 2);

            gate_info->OverlappedEx[1].nOvFlag = OVERLAPPED_SEND;

            // 首次 Recv 前持有引用，防止连接在 IO 完成前被释放
            gate_info->add_ref();
            if (gate_info->recv() == SOCKET_ERROR
                && WSAGetLastError() != ERROR_IO_PENDING) {
                // 投递失败：释放引用并关闭
                LOG_INFO("新连接首次Recv投递失败");
                gate_info->release();
                gate_info->close_connection();
            }
        }
    }

    // 释放 accept 重叠结构，并重新投递一个 AcceptEx 保持预投递数量
    delete lpAcceptOv;
    post_accept_ex();
}

// Signal all IOCP worker threads to exit by posting completion packets
void signal_iocp_threads_exit()
{
    if (g_iocp == NULL) return;

    std::lock_guard<std::mutex> lock(g_worker_threads_mutex);
    size_t threadCount = g_worker_threads.size();

    // Post one completion packet per worker thread to wake them from
    // GetQueuedCompletionStatus
    for (size_t i = 0; i < threadCount; i++) {
        PostQueuedCompletionStatus(g_iocp, 0, 0, NULL);
    }
}

// Wait for all worker threads to exit
void wait_for_all_worker_threads(DWORD timeoutMs)
{
    std::vector<HANDLE> threadsToWait;
    {
        std::lock_guard<std::mutex> lock(g_worker_threads_mutex);
        threadsToWait = g_worker_threads;
    }

    if (threadsToWait.empty()) return;

    // Filter out invalid handles and duplicates
    std::vector<HANDLE> validHandles;
    validHandles.reserve(threadsToWait.size());

    for (HANDLE h : threadsToWait) {
        // Skip NULL and obviously invalid handles (like freed memory pattern
        // 0xfdfdfdfd)
        if (h == NULL || h == INVALID_HANDLE_VALUE
            || (ULONG_PTR)h == 0xfdfdfdfd) {
            continue;
        }

        // Check for duplicates
        bool isDuplicate = false;
        for (HANDLE existing : validHandles) {
            if (existing == h) {
                isDuplicate = true;
                break;
            }
        }

        if (!isDuplicate) {
            validHandles.push_back(h);
        }
    }

    if (validHandles.empty()) {
        // Still clear the original list
        {
            std::lock_guard<std::mutex> lock(g_worker_threads_mutex);
            g_worker_threads.clear();
        }

        // Close IOCP handle
        if (g_iocp) {
            CloseHandle(g_iocp);
            g_iocp = NULL;
        }
        return;
    }

    // Wait for all threads
    DWORD result = WaitForMultipleObjects(
        static_cast<DWORD>(validHandles.size()), validHandles.data(),
        TRUE,  // Wait for all
        timeoutMs);

    if (result == WAIT_TIMEOUT) {
        LOG_FATAL("Timeout waiting for worker threads to exit");
    }
    else if (result == WAIT_FAILED) {
        LOG_ERROR("WaitForMultipleObjects failed: %lu", GetLastError());
    }

    // Close thread handles
    for (HANDLE hThread : validHandles) {
        CloseHandle(hThread);
    }

    {
        std::lock_guard<std::mutex> lock(g_worker_threads_mutex);
        g_worker_threads.clear();
    }

    // Close IOCP handle
    if (g_iocp) {
        CloseHandle(g_iocp);
        g_iocp = NULL;
    }
}
