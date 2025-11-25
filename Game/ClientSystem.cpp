#include "ClientSystem.h"
#include <cstdio>

ClientSystem::ClientSystem() : sock(INVALID_SOCKET), hRecvThread(NULL), my_player_id(-1) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[Error] WSAStartup() failed\n");
    }
    InitializeCriticalSection(&m_map_cs);
    maprecvEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

ClientSystem::~ClientSystem() {
    DeleteCriticalSection(&m_map_cs);
    Disconnect();
    WSACleanup();
}

bool ClientSystem::Connect(const char* server_ip, u_short port) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("[Error] socket() failed\n");
        return false;
    }

    SOCKADDR_IN server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    if (connect(sock, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("[Error] connect() failed\n");
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }

    printf("[Info] Connected to server at %s:%d\n", server_ip, port);
    return true;
}

void ClientSystem::Disconnect() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    if (hRecvThread != NULL) {
        CloseHandle(hRecvThread);
        hRecvThread = NULL;
    }
}

void ClientSystem::StartRecvThread() {
    hRecvThread = CreateThread(NULL, 0, ClientRecvThread, this, 0, NULL);
}

DWORD WINAPI ClientSystem::ClientRecvThread(LPVOID lpParam) {
    ClientSystem* client = (ClientSystem*)lpParam;
    while (client->sock != INVALID_SOCKET) {
        if (!client->DoRecv()) {
            break; // 오류 또는 연결 끊김
        }
    }
    printf("[Info] Receive thread finished.\n");
    return 0;
}


bool ClientSystem::DoRecv() {
    int recv_len = recv(sock, recv_buffer, sizeof(recv_buffer), 0);

    if (recv_len == SOCKET_ERROR) {
        printf("[Error] recv() failed with error: %d\n", WSAGetLastError());
        Disconnect();
        return false;
    }
    if (recv_len == 0) {
        printf("[Info] Server disconnected.\n");
        Disconnect();
        return false;
    }

    printf("[Debug] Received %d bytes.\n", recv_len);
    ProcessPacket(recv_buffer);
    return true;
}

void ClientSystem::ProcessPacket(char* packet_buf) {
    BasePacket* base_p = (BasePacket*)packet_buf;
    
    printf("[Debug] Processing packet type: %d\n", base_p->type);

    switch (base_p->type) {
        case SC_ASSIGN_ID: {
            SC_AssignIDPacket* p = (SC_AssignIDPacket*)packet_buf;
            p->Decode(); // 네트워크 바이트 순서에서 호스트 바이트 순서로 변환
            p->Log();    // 디버깅용
            HandleAssignID(p);
            break;
        }
        case SC_EVENT: {
            SC_EventPacket* p = (SC_EventPacket*)packet_buf;
            p->Decode(); // 네트워크 바이트 순서에서 호스트 바이트 순서로 변환
            p->Log();    // 디버깅용
            HandleEvent(p);
            break;
        }
        case SC_MAP_INFO: {
            SC_MapInfoPacket* p = (SC_MapInfoPacket*)packet_buf;
            p->Decode(); // 네트워크 바이트 순서에서 호스트 바이트 순서로 변환
            p->Log();    // 디버깅용
            HandleMapInfo(p);
            break;
        }
        // 다른 패킷 처리 케이스를 여기에 추가
        default: {
            printf("[Warning] Received unknown packet type: %d\n", base_p->type);
            break;
        }
    }
}

void ClientSystem::HandleAssignID(SC_AssignIDPacket* packet) {
    my_player_id = packet->player_id;
    printf("[Info] Assigned Player ID: %hu\n", my_player_id);
    // 이제 'my_player_id'를 사용하여 이 클라이언트를 식별할 수 있습니다
}

void ClientSystem::HandleEvent(SC_EventPacket* packet) {
    printf("[Info] Handling Event: ");
    switch (packet->event_type) {
        case STAGE_CLEAR:
            printf("STAGE_CLEAR\n");
            // TODO: 스테이지 클리어 UI 표시, 사운드 재생 등
            break;
        case GAME_WIN:
            printf("GAME_WIN\n");
            // TODO: 게임 승리 UI 표시, 게임 종료 처리 등
            break;
        default:
            printf("Unknown Event Type: %d\n", packet->event_type);
            break;
    }
}

void ClientSystem::HandleMapInfo(SC_MapInfoPacket* packet)
{
    EnterCriticalSection(&m_map_cs);

    m_map = packet->mapInfo;

    LeaveCriticalSection(&m_map_cs);

    SetEvent(maprecvEvent);
}

Map ClientSystem::GetMap()
{
    Map temp_map;

    WaitForSingleObject(maprecvEvent, INFINITE);

    EnterCriticalSection(&m_map_cs);
    temp_map = m_map;
    LeaveCriticalSection(&m_map_cs);

    return temp_map;
}

void ClientSystem::SendUploadMapPacket(CS_UploadMapPacket* packet)
{
    packet->Encode();
    int sent = send(sock, (char*)packet, sizeof(CS_UploadMapPacket), 0);
    printf("[CLIENT] Sent CS_UploadMapPacket: %d bytes (sizeof=%zu)\n", sent, sizeof(CS_UploadMapPacket));
}

void ClientSystem::SendStartSessionRequestPacket(CS_StartSessionRequestPacket* packet)
{
    packet->Encode();
    int sent = send(sock, (char*)packet, sizeof(CS_StartSessionRequestPacket), 0);
    printf("[CLIENT] Sent CS_StartSessionRequestPacket: %d bytes (sizeof=%zu)\n", sent, sizeof(CS_StartSessionRequestPacket));
}

bool ClientSystem::SendPlayerUpdatePacket(Player* player)
{
    if (sock == INVALID_SOCKET) {
        return false;
    }
    CS_PlayerUpdatePacket packet(my_player_id, player);
    packet.Encode();

    int sent_bytes = send(sock, (char*)&packet, sizeof(CS_PlayerUpdatePacket), 0);
    if (sent_bytes == SOCKET_ERROR) {
        printf("[Error] send() CS_PlayerUpdatePacket failed: %d\n", WSAGetLastError());
        return false;
    }
    return true;
}

bool ClientSystem::SendEndSessionRequestPacket() {
    if (sock == INVALID_SOCKET) {
        printf("[Error] Cannot send CS_EndSessionRequestPacket: Not connected to server.\n");
        return false;
    }
    if (my_player_id == (u_short)-1) {
        printf("[Error] Cannot send CS_EndSessionRequestPacket: Player ID not assigned.\n");
        return false;
    }

    CS_EndSessionRequestPacket packet(my_player_id); // Create packet with player ID
    packet.Encode(); // Convert to network byte order

    int sent_bytes = send(sock, (char*)&packet, sizeof(CS_EndSessionRequestPacket), 0);
    if (sent_bytes == SOCKET_ERROR) {
        printf("[Error] send() CS_EndSessionRequestPacket failed: %d\n", WSAGetLastError());
        return false;
    }
    if (sent_bytes != sizeof(CS_EndSessionRequestPacket)) {
        printf("[Warning] Sent %d bytes for CS_EndSessionRequestPacket, expected %zu\n", sent_bytes, sizeof(CS_EndSessionRequestPacket));
    }
    
    printf("[Info] Sent CS_EndSessionRequestPacket for player ID %hu, %d bytes.\n", my_player_id, sent_bytes);
    return true;
}