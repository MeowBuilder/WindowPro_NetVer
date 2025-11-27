#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <thread>
#include <string>
#include "ServerSystem.h"

// ------------------------------
// 전역 관리 변수 (임계영역 보호)
// ------------------------------
CRITICAL_SECTION g_cs;
bool g_running = true;

// ------------------------------
// g_running 읽기/쓰기 함수
// ------------------------------
bool IsRunning()
{
    EnterCriticalSection(&g_cs);
    bool r = g_running;
    LeaveCriticalSection(&g_cs);
    return r;
}

void SetRunning(bool v)
{
    EnterCriticalSection(&g_cs);
    g_running = v;
    LeaveCriticalSection(&g_cs);
}

// ------------------------------
// Client Accept Thread
// ------------------------------
void AcceptThread(ServerSystem* server)
{
    while (IsRunning())
    {
        if (!server->AcceptClient())
        {
            // accept 실패 시 CPU 폭주 방지
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    printf("[SERVER] AcceptThread 종료\n");
}

// ------------------------------
// main()
// ------------------------------
int main()
{
    // 임계영역 초기화
    InitializeCriticalSection(&g_cs);

    ServerSystem server;

    const u_short PORT = 9000;

    // 1) 서버 시작
    if (!server.Start(PORT))
    {
        printf("[SERVER] 서버 시작 실패!\n");
        DeleteCriticalSection(&g_cs);
        return -1;
    }

    printf("[SERVER] Server started on port %d\n", PORT);

    // 2) 클라이언트 accept 스레드 시작
    std::thread th(AcceptThread, &server);
    th.detach();

    // 3) 콘솔 명령 처리 (종료)
    printf("[SERVER] 종료하려면 'quit' 입력\n");

    while (IsRunning())
    {
        std::string cmd;
        getline(std::cin, cmd);

        if (cmd == "quit" || cmd == "exit")
        {
            printf("[SERVER] Shutting down...\n");
            SetRunning(false);
            break;
        }
    }

    // ServerSystem 소멸자에서 소켓 정리됨

    // 임계영역 해제
    DeleteCriticalSection(&g_cs);

    printf("[SERVER] Server stopped.\n");
    return 0;
}