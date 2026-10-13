#include <algorithm>
#include <mutex>
#include "../engine/HeartBeat.h"
#include "Container.h"
#include "GameSvr.h"
#include "GateRecvDispatch.h"
#include "Global.h"
#include "NetCompletion.h"
#include "ServerSockHandler.h"

bool g_terminated = false;
HANDLE g_event;

SOCKET g_ssock = INVALID_SOCKET;
SOCKADDR_IN g_saddr;

SOCKET g_csock = INVALID_SOCKET;
SOCKADDR_IN g_caddr;

SOCKET g_clsock = INVALID_SOCKET;
SOCKADDR_IN g_claddr;

std::list<CGateInfo*> g_gate_list;
std::mutex g_gate_list_mutex;

extern void on_offline(long objectid);
extern bool splite_command_parse(long userid, char* Comm, int len);
void call_heart_beat();
void user_command_loop();
void close_client(uint32_t serial);

DWORD WINAPI server_worker_thread(LPVOID CompletionPortID)
{
    DWORD dwBytesTransferred;
    ULONG_PTR ulCompletionKey;
    LPOVERLAPPED lpOverlapped;

    while (true) {
        // 检查返回值：失败时若 lpOverlapped 为 NULL 则跳过，避免解引用无效指针
        bool ok = GetQueuedCompletionStatus(
            (HANDLE)CompletionPortID, &dwBytesTransferred, &ulCompletionKey,
            &lpOverlapped, INFINITE);

        if (g_terminated) return 0L;

        const bool hasOverlapped = lpOverlapped != NULL;
        const int ovFlag
            = hasOverlapped
                  ? reinterpret_cast<LPOVERLAPPEDEX>(lpOverlapped)->nOvFlag
                  : 0;

        switch (classify_completion(ok, hasOverlapped, dwBytesTransferred,
                                    ulCompletionKey, ovFlag)) {
            // 关停信号
            case WorkerAction::Exit:
                return 0L;

            // 无效或未知完成包，跳过
            case WorkerAction::Skip:
                continue;

            // accept 完成：创建新连接并重新预投
            case WorkerAction::Accept:
                handle_accept_completion(
                    reinterpret_cast<LPACCEPTOVERLAPPED>(lpOverlapped));
                continue;

            // 发送完成：释放缓冲与引用，恢复发送通道并继续队列
            case WorkerAction::SendDone: {
                LPOVERLAPPEDSEND sendBuf
                    = reinterpret_cast<LPOVERLAPPEDSEND>(lpOverlapped);
                CGateInfo* gate_info
                    = reinterpret_cast<CGateInfo*>(ulCompletionKey);
                CGateInfo* gate = sendBuf->gate ? sendBuf->gate : gate_info;

                delete[] sendBuf->DataBuf.buf;
                delete sendBuf;

                InterlockedExchange(&gate->m_do_sending, 0);  // 释放发送通道
                gate->try_send_next();  // 发送下一条（内部 AddRef）
                gate->release();        // 释放本次发送的引用
                continue;
            }

            // 接收完成
            case WorkerAction::RecvDone: {
                CGateInfo* gate_info
                    = reinterpret_cast<CGateInfo*>(ulCompletionKey);

                // 断开：统一关闭路径，不再 re-arm，释放本次 recv 的引用
                if (dwBytesTransferred == 0) {
                    LOG_INFO("对端关闭连接(recv=0): player=%d",
                             gate_info->m_player);
                    gate_info->close_connection();
                    gate_info->release();
                    continue;
                }

                // 对端 IP 随登录消息一起入队
                char peerIp[INET_ADDRSTRLEN] = "";
                struct sockaddr_in sa;
                int saLen = sizeof(sa);
                if (getpeername(gate_info->m_sock,
                                reinterpret_cast<struct sockaddr*>(&sa), &saLen)
                    == 0) {
                    inet_ntop(AF_INET, &sa.sin_addr, peerIp, sizeof(peerIp));
                }

                if (dispatch_gate_recv(gate_info, dwBytesTransferred, peerIp)
                    == RecvAction::release) {
                    gate_info->release();
                    continue;
                }

                // re-arm 接收：成功则引用转移给新 recv（保持持有）；失败则释放
                if (gate_info->recv() == SOCKET_ERROR
                    && WSAGetLastError() != ERROR_IO_PENDING) {
                    LOG_INFO("Recv投递失败断开: player=%d",
                             gate_info->m_player);
                    LOG_ERROR("WSARecv() failed");
                    gate_info->close_connection();
                    gate_info->release();
                }
                continue;
            }
        }
    }

    return 0;
}

CGateInfo* OnDeleteGate[1024];

void process_user_message()
{
    int dcount = 0;

    if (!g_gate_list.empty()) {
        memset(OnDeleteGate, 0x0, sizeof(CGateInfo*) * 1024);

        // 遍历时加锁，避免与 worker 线程的增删冲突
        std::lock_guard<std::mutex> lock(g_gate_list_mutex);

        for (auto* gate_info : g_gate_list) {
            if (gate_info) {  // 处理每个客户端的消息
                _LPTRECVBUFF lpRecvBuff
                    = (_LPTRECVBUFF)gate_info->m_recv_buff_q.pop_q();

                while (lpRecvBuff)  // 存在消息
                {
                    switch (lpRecvBuff->type) {
                        case RECV_MSG_LOGIN: {
                            std::lock_guard<std::recursive_mutex> lock(
                                gate_info->m_cs);
                            if (gate_info->m_login == GATE_LOGIN_LOGGING_IN) {
                                if (!on_login((uint32_t)gate_info,
                                              gate_info->m_sock,
                                              lpRecvBuff->szData)) {
                                    close_client((uint32_t)gate_info);
                                }
                            }
                        } break;
                        case RECV_MSG_COMMAND: {
                            std::lock_guard<std::recursive_mutex> lock(
                                gate_info->m_cs);
                            if (gate_info->m_login == GATE_LOGIN_NORMAL) {
                                int32_t player = gate_info->m_player;
                                splite_command_parse(player, lpRecvBuff->szData,
                                                     lpRecvBuff->len);
                            }
                        } break;
                        case RECV_MSG_EXIT: {
                            std::lock_guard<std::recursive_mutex> lock(
                                gate_info->m_cs);
                            if (gate_info->m_login
                                > GATE_LOGIN_PENDING_DELETE) {
                                int32_t player     = gate_info->m_player;
                                gate_info->m_login = GATE_LOGIN_PENDING_DELETE;

                                if (player) on_offline(player);

                                if (dcount < 1024) {
                                    OnDeleteGate[dcount] = gate_info;
                                    gate_info->m_login
                                        = GATE_LOGIN_SCHEDULED_DELETE;
                                    dcount++;
                                }
                            }
                        } break;
                    }
                    delete lpRecvBuff;
                    lpRecvBuff = NULL;
                    lpRecvBuff = (_LPTRECVBUFF)gate_info->m_recv_buff_q.pop_q();
                }

                {
                    std::lock_guard<std::recursive_mutex> lock(gate_info->m_cs);
                    if (gate_info->m_login == GATE_LOGIN_PENDING_DELETE
                        && dcount < 1024) {
                        gate_info->m_login   = GATE_LOGIN_SCHEDULED_DELETE;
                        OnDeleteGate[dcount] = gate_info;
                        dcount++;
                    }
                }
            }
        }  // for
    }  // lock_guard scope ends here, mutex automatically released

    // 删除所有待删除的对象
    flush_destruct_list();

    // 释放待删除连接：从列表移除并释放列表引用（IO 引用归零后才真正 delete）
    for (int i = 0; i < dcount; i++) {
        auto it = std::find(g_gate_list.begin(), g_gate_list.end(),
                            OnDeleteGate[i]);
        if (it != g_gate_list.end()) {
            g_gate_list.erase(it);
        }
        OnDeleteGate[i]->release();
        OnDeleteGate[i] = NULL;
    }
}

DWORD WINAPI main_worker_thread(LPVOID lpParameter)
{
    static uint32_t last_tick = GetTickCount();
    static uint32_t nBytes    = 0;

    while (true) {
        if (g_terminated) {
            exit_game();
            dis_connect_database();
            SetEvent(g_event);
            return 0L;
        }
        process_user_message();
        user_command_loop();

        if (GetTickCount() - last_tick >= 1000) {
            last_tick = GetTickCount();
            call_heart_beat();
        }

        Sleep(1);  // sleep 1 ms
    }
}
