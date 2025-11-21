#include "ServerSystem.h"

#pragma region

ServerSystem::ServerSystem() : m_listen(INVALID_SOCKET)
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
        m_clients[i] = INVALID_SOCKET;

    InitializeCriticalSection(&m_cs);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        printf("[Error] WSAStartup() failed\n");
}

ServerSystem::~ServerSystem()
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (m_clients[i] != INVALID_SOCKET)
        {
            closesocket(m_clients[i]);
            m_clients[i] = INVALID_SOCKET;
        }
    }

    if (m_listen != INVALID_SOCKET)
    {
        closesocket(m_listen);
        m_listen = INVALID_SOCKET;
    }

    WSACleanup();
    DeleteCriticalSection(&m_cs);
}

#pragma endregion



#pragma region   // Server Start & Accept

bool ServerSystem::Start(u_short port)
{
    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET)
    {
        printf("[Error] socket() failed\n");
        return false;
    }

    SOCKADDR_IN addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_listen, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        printf("[Error] bind() failed\n");
        closesocket(m_listen);
        return false;
    }

    if (listen(m_listen, MAX_PLAYERS) == SOCKET_ERROR)
    {
        printf("[Error] listen() failed\n");
        closesocket(m_listen);
        return false;
    }

    printf("[SERVER] Listening on port %d ...\n", port);
    return true;
}

bool ServerSystem::AcceptClient()
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (m_clients[i] == INVALID_SOCKET)
        {
            SOCKET client_sock = accept(m_listen, NULL, NULL);
            if (client_sock == INVALID_SOCKET)
            {
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



#pragma region   // Recv Thread & Packet Processing

void ServerSystem::StartRecvThread(int client_id)
{
    std::thread([this, client_id]
    {
        while (m_clients[client_id] != INVALID_SOCKET)
        {
            if (!DoRecv(client_id))
                break;
        }

        printf("[SERVER] Recv thread for client %d ended.\n", client_id);

        EnterCriticalSection(&m_cs);
        if (m_clients[client_id] != INVALID_SOCKET)
        {
            closesocket(m_clients[client_id]);
            m_clients[client_id] = INVALID_SOCKET;
        }
        LeaveCriticalSection(&m_cs);

    }).detach();
}


bool ServerSystem::DoRecv(int client_id)
{
    char recv_buffer[4096];

    int recv_len = recv(m_clients[client_id], recv_buffer, sizeof(recv_buffer), 0);

    if (recv_len <= 0)
    {
        printf("[SERVER] Client %d disconnected.\n", client_id);
        return false;
    }

    ProcessPacket(recv_buffer, client_id);
    return true;
}

void ServerSystem::ProcessPacket(char* packet, int client_id)
{
    BasePacket* base_p = (BasePacket*)packet;

    printf("[SERVER] Received packet type: %d from client %d\n",
           base_p->type, client_id);

    switch (base_p->type)
    {
        case CS_UPLOAD_MAP:
        {
            auto* p = (CS_UploadMapPacket*)packet;
            p->Decode();
            p->Log();
            HandleMapUpload(p, client_id);
            break;
        }

        case CS_START_SESSION_REQ:
        {
            auto* p = (CS_StartSessionRequestPacket*)packet;
            p->Decode();
            p->Log();

            // 클라이언트 패킷에는 map_number가 없음 → 기본 0으로 처리
            HandleStartSessionRequest(0, client_id);
            break;
        }

        default:
            printf("[Warning] Unknown packet type %d\n", base_p->type);
            break;
    }
}

#pragma endregion



#pragma region   // Map Upload / Start Session

void ServerSystem::HandleMapUpload(CS_UploadMapPacket* packet, int client_id)
{
    printf("[SERVER] Handling map upload from client %d\n", client_id);

    server_map.block_count = packet->block_count;
    memcpy(server_map.blocks, packet->blocks, sizeof(packet->blocks));

    server_map.object_count = packet->object_count;
    memcpy(server_map.objects, packet->objects, sizeof(packet->objects));

    server_map.enemy_count = packet->enemy_spawn_count;
    for (int i = 0; i < packet->enemy_spawn_count; i++)
        server_map.enemys[i] = Make_Enemy(packet->enemy_spawns[i].x, packet->enemy_spawns[i].y, 0);

    for (int i = 0; i < 3; i++)
    {
        server_map.P_start_x[i] = packet->player_start_pos[i].x;
        server_map.P_start_y[i] = packet->player_start_pos[i].y;
    }

    SendMapUploadResponsePacket(client_id, true);

    // 모든 클라이언트에 전송
    BroadcastMapInfo();
}

void ServerSystem::HandleStartSessionRequest(int map_number, int client_id)
{
    printf("[SERVER] StartSessionRequest (client %d, map_number=%d)\n",
           client_id, map_number);

    // 기본 맵
    LoadDefaultMap(map_number);

    BroadcastMapInfo();
}

void ServerSystem::BroadcastMapInfo()
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (m_clients[i] != INVALID_SOCKET)
        {
            SC_MapInfoPacket info;

            info.block_count = server_map.block_count;
            memcpy(info.blocks, server_map.blocks, sizeof(info.blocks));

            info.object_count = server_map.object_count;
            memcpy(info.objects, server_map.objects, sizeof(info.objects));

            info.enemy_spawn_count = server_map.enemy_count;

            for (int e = 0; e < server_map.enemy_count; e++)
            {
                info.enemy_spawns[e].x = server_map.enemys[e].x;
                info.enemy_spawns[e].y = server_map.enemys[e].y;
            }

            for (int p = 0; p < 3; p++)
            {
                info.player_start_pos[p].x = server_map.P_start_x[p];
                info.player_start_pos[p].y = server_map.P_start_y[p];
            }

            SendMapInfoPacket(i, &info);
        }
    }
}

bool ServerSystem::SendMapUploadResponsePacket(int client_id, bool success)
{
    SC_MapUploadResponsePacket p(success);
    p.Encode();
    send(m_clients[client_id], (char*)&p, sizeof(p), 0);
    return true;
}

bool ServerSystem::SendMapInfoPacket(int client_id, SC_MapInfoPacket* packet)
{
    packet->Encode();

    int sent = send(m_clients[client_id],
                    (char*)packet,
                    sizeof(*packet), 0);

    if (sent == SOCKET_ERROR)
    {
        printf("[Error] Failed sending MapInfo to client %d\n", client_id);
        return false;
    }

    packet->Log();
    return true;
}

#pragma endregion



#pragma region   // Default Map Load

void ServerSystem::LoadDefaultMap(int map_number)
{
    printf("[SERVER] init_map(%d) 호출\n", map_number);

    RECT desk_rt = {0, 0, 1920, 1080};
    Player dummy_players[3];

    init_map(server_map, desk_rt, dummy_players, map_number);
}

#pragma endregion
