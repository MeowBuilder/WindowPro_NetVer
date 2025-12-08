#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include "Packets.h"

#pragma comment(lib, "ws2_32.lib")

// 전역 변수 선언 (ClientSystem에서 접근하기 위함)
extern HWND g_hIpInputDlg;

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
    Map GetMap(int index = -1);
    void SwitchToNextMap();
    int GetCurrentMapIndex() const { return current_map_index; }

    Player* getPlayer(int player_id) { return &players[player_id]; };
    SOCKET getSocket() { return sock; };

    // 게임 상태
    u_short my_player_id;
    bool StartGame;
    bool Mapready;

    static const int RECV_STREAM_BUFFER_SIZE = 65536;

    char	m_streamBuf[RECV_STREAM_BUFFER_SIZE];
    int		m_streamLen;
private:
    // 네트워크
    SOCKET sock;
    HANDLE hRecvThread;
    char recv_buffer[12000];
    Map m_maps[4];
    int current_map_index;
    CRITICAL_SECTION m_map_cs;

    HANDLE maprecvEvent;

    Player players[3];


    // 패킷 처리
    bool DoRecv();
    void ProcessPacket(char* packet);
    void HandleAssignID(SC_AssignIDPacket* packet);
    void HandleEvent(SC_EventPacket* packet);
    void HandleMapInfo(SC_MapInfoPacket* packet);
    void HandleMapUploadResponse(SC_MapUploadResponsePacket* packet);
    void HandlePlayerJoin(SC_PlayerJoinPacket* packet);
    void HandleGameState(SC_GameStatePacket* packet);

    // 스레드 함수
    static DWORD WINAPI ClientRecvThread(LPVOID lpParam);
};
