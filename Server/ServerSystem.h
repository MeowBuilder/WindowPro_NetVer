#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <cstdio>

#include "Packets.h"
#include "GameManager.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX_PLAYERS 3

class ServerSystem
{
public:
    ServerSystem();
    ~ServerSystem();

    bool Start(u_short port);
    bool AcceptClient();
    bool DoRecv(int client_id);
    void ProcessPacket(char* packet, int client_id);

    // 패킷 처리
    void HandleMapUpload(CS_UploadMapPacket* packet, int client_id);
    void HandleStartSessionRequest(int client_id);  // map_number 제거된 버전

    bool SendMapUploadResponsePacket(int client_id, bool is_success);
    bool SendMapInfoPacket(int client_id, SC_MapInfoPacket* packet);
    bool SendAssignIDPacket(int client_id, u_short id);
    bool SendEventPacket(int client_id, E_EventType event_type);

private:
    SOCKET m_listen;
    SOCKET m_clients[MAX_PLAYERS];
    CRITICAL_SECTION m_cs;

    void StartRecvThread(int client_id);
    void BroadcastMapInfo();

    Map server_map;
    void LoadDefaultMap(int map_num);
};
