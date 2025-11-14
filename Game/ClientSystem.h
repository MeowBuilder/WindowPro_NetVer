#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include "Packets.h"

#pragma comment(lib, "ws2_32.lib")

class ClientSystem {
public:
    ClientSystem();
    ~ClientSystem();

    bool Connect(const char* server_ip, u_short port);
    void Disconnect();
    void StartRecvThread();

private:
    // 네트워크
    SOCKET sock;
    HANDLE hRecvThread;
    char recv_buffer[4096];

    // 게임 상태
    u_short my_player_id;

    // 패킷 처리
    bool DoRecv();
    void ProcessPacket(char* packet);
    void HandleAssignID(SC_AssignIDPacket* packet);
    void SendUploadMapPacket(CS_UploadMapPacket* packet);
    void SendStartSessionRequestPacket(CS_StartSessionRequestPacket* packet);

    // 스레드 함수
    static DWORD WINAPI ClientRecvThread(LPVOID lpParam);
};
