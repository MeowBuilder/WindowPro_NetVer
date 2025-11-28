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

    void SendUploadMapPacket(CS_UploadMapPacket* packet);
    void SendStartSessionRequestPacket(CS_StartSessionRequestPacket* packet);
    bool SendPlayerUpdatePacket(Player* player);
    bool SendEndSessionRequestPacket();
    Map GetMap();

    Player getPlayer(int player_id) { return players[player_id]; };

    // 게임 상태
    u_short my_player_id;
private:
    // 네트워크
    SOCKET sock;
    HANDLE hRecvThread;
    char recv_buffer[12000];
    Map m_map;
    CRITICAL_SECTION m_map_cs;

    HANDLE maprecvEvent;

    Player players[3];

    // 패킷 처리
    bool DoRecv();
    void ProcessPacket(char* packet);
    void HandleAssignID(SC_AssignIDPacket* packet);
    void HandleEvent(SC_EventPacket* packet);
    void HandleMapInfo(SC_MapInfoPacket* packet);

    // 스레드 함수
    static DWORD WINAPI ClientRecvThread(LPVOID lpParam);
};
