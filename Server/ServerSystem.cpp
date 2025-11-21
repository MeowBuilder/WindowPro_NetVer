#include "ServerSystem.h"

/*
 * ===========================================================
 *  ServerSystem.cpp
 * ===========================================================
 */

#pragma region

ServerSystem::ServerSystem() : m_listen(INVALID_SOCKET) {
    for (int i = 0; i < MAX_PLAYERS; ++i)
        m_clients[i] = INVALID_SOCKET;

    InitializeCriticalSection(&m_cs);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        printf("[Error] WSAStartup() failed\n");
}

ServerSystem::~ServerSystem() {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_clients[i] != INVALID_SOCKET) {
            closesocket(m_clients[i]);
            m_clients[i] = INVALID_SOCKET;
        }
    }

    if (m_listen != INVALID_SOCKET) {
        closesocket(m_listen);
        m_listen = INVALID_SOCKET;
    }

    WSACleanup();
    DeleteCriticalSection(&m_cs);
}

#pragma endregion


#pragma region

bool ServerSystem::Start(u_short port) {
    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET) {
        printf("[Error] socket() failed\n");
        return false;
    }

    SOCKADDR_IN addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_listen, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[Error] bind() failed\n");
        closesocket(m_listen);
        return false;
    }

    if (listen(m_listen, MAX_PLAYERS) == SOCKET_ERROR) {
        printf("[Error] listen() failed\n");
        closesocket(m_listen);
        return false;
    }

    printf("[SERVER] Listening on port %d ...\n", port);
    return true;
}

bool ServerSystem::AcceptClient() {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_clients[i] == INVALID_SOCKET) {
            SOCKET client_sock = accept(m_listen, NULL, NULL);
            if (client_sock == INVALID_SOCKET) {
                printf("[Error] accept() failed (%d)\n", WSAGetLastError());
                return false;
            }

            EnterCriticalSection(&m_cs);
            m_clients[i] = client_sock;
            LeaveCriticalSection(&m_cs);

            printf("[SERVER] Client %d connected.\n", i);

            SendAssignIDPacket(i, i);
            StartRecvThread(i);
            return true;
        }
    }
    return false;
}

#pragma endregion


#pragma region

void ServerSystem::StartRecvThread(int client_id) {
    std::thread([this, client_id] {
        while (m_clients[client_id] != INVALID_SOCKET) {
            if (!DoRecv(client_id))
                break;
        }
        printf("[SERVER] Recv thread for client %d ended.\n", client_id);
    }).detach();
}

bool ServerSystem::DoRecv(int client_id) {
    char recv_buffer[4096];
    int recv_len = recv(m_clients[client_id], recv_buffer, sizeof(recv_buffer), 0);

    if (recv_len <= 0) {
        printf("[SERVER] Client %d disconnected.\n", client_id);
        closesocket(m_clients[client_id]);
        m_clients[client_id] = INVALID_SOCKET;
        return false;
    }

    ProcessPacket(recv_buffer, client_id);
    return true;
}

void ServerSystem::ProcessPacket(char* packet, int client_id) {
    BasePacket* base_p = (BasePacket*)packet;
    printf("[SERVER] Received packet type: %d from client %d\n", base_p->type, client_id);

    switch (base_p->type) {
    case CS_UPLOAD_MAP: {
        auto* map_packet = (CS_UploadMapPacket*)packet;
        map_packet->Decode();
        map_packet->Log();
        HandleMapUpload(map_packet, client_id);
        break;
    }
    case CS_START_SESSION_REQ: {
        auto* start_packet = (CS_StartSessionRequestPacket*)packet;
        start_packet->Decode();
        start_packet->Log();
        HandleStartSessionRequest(start_packet, client_id);
        break;
    }
    default:
        printf("[Warning] Unknown packet type: %d\n", base_p->type);
        break;
    }
}

#pragma endregion


#pragma region

void ServerSystem::HandleMapUpload(CS_UploadMapPacket* packet, int client_id) {
    printf("[SERVER] Handling CS_UploadMapPacket from client %d\n", client_id);

    //
    // 제작맵: 클라이언트 업로드 → server_map에 저장
    //
    server_map.block_count = packet->block_count;
    memcpy(server_map.blocks, packet->blocks, sizeof(packet->blocks));

    server_map.object_count = packet->object_count;
    memcpy(server_map.objects, packet->objects, sizeof(packet->objects));

    server_map.enemy_count = packet->enemy_spawn_count;
    for (int i = 0; i < packet->enemy_spawn_count; i++)
        server_map.enemys[i] = Make_Enemy(packet->enemy_spawns[i].x, packet->enemy_spawns[i].y, 0);

    for (int i = 0; i < 3; i++) {
        server_map.P_start_x[i] = packet->player_start_pos[i].x;
        server_map.P_start_y[i] = packet->player_start_pos[i].y;
    }

    SendMapUploadResponsePacket(client_id, true);

    //
    // 맵 정보 전체 전송
    //
    SC_MapInfoPacket info;
    memcpy(&info, &server_map, sizeof(info));

    for (int i = 0; i < MAX_PLAYERS; i++)
        if (m_clients[i] != INVALID_SOCKET)
            SendMapInfoPacket(i, &info);
}

bool ServerSystem::SendMapUploadResponsePacket(int client_id, bool is_success) {
    SC_MapUploadResponsePacket rsp(is_success);
    rsp.Encode();

    int sent = send(m_clients[client_id], (char*)&rsp, sizeof(rsp), 0);
    return (sent != SOCKET_ERROR);
}

void ServerSystem::HandleStartSessionRequest(CS_StartSessionRequestPacket* packet, int client_id) {
    printf("[SERVER] Handling CS_StartSessionRequestPacket from client %d\n", client_id);
    printf("[SERVER] map_number = %d\n", packet->map_number);

    //
    // map_number == 0 → 제작맵 (UploadMapPacket을 기다림)
    //
    if (packet->map_number == 0) {
        printf("[SERVER] 제작 맵 모드. UploadMapPacket을 기다립니다.\n");
        return;
    }

    //
    // 기본 맵 → init_map 사용하여 server_map 로드
    //
    LoadDefaultMap(packet->map_number);

    //
    // 전체 클라에 MapInfo 전송
    //
    SC_MapInfoPacket info;
    memcpy(&info, &server_map, sizeof(info));

    for (int i = 0; i < MAX_PLAYERS; i++)
        if (m_clients[i] != INVALID_SOCKET)
            SendMapInfoPacket(i, &info);
}

bool ServerSystem::SendMapInfoPacket(int client_id, SC_MapInfoPacket* packet) {
    packet->Encode();
    int sent = send(m_clients[client_id], (char*)packet, sizeof(*packet), 0);

    if (sent == SOCKET_ERROR) {
        printf("[Error] send() failed (client %d)\n", client_id);
        return false;
    }
    packet->Log();
    return true;
}

bool ServerSystem::SendAssignIDPacket(int client_id, u_short id) {
    SC_AssignIDPacket assign(id);
    assign.Encode();

    send(m_clients[client_id], (char*)&assign, sizeof(assign), 0);
    return true;
}

bool ServerSystem::SendEventPacket(int client_id, E_EventType event_type) {
    SC_EventPacket pkt(event_type);
    pkt.Encode();

    send(m_clients[client_id], (char*)&pkt, sizeof(pkt), 0);
    return true;
}

#pragma endregion


#pragma region

//
// init_map을 서버에서 사용 가능하게 만드는 함수 
//
void ServerSystem::LoadDefaultMap(int map_number)
{
    printf("[SERVER] init_map() 호출: map_number = %d\n", map_number);

    RECT desk_rt = {0, 0, 1920, 1080}; // 서버 고정 RECT
    Player dummy_players[3];            // 더미 플레이어

    init_map(server_map, desk_rt, dummy_players, map_number);
}

#pragma endregion
