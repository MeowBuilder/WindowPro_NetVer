#include "ServerSystem.h"

#pragma region Constructor / Destructor

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



#pragma region Start & Accept

bool ServerSystem::Start(u_short port)
{
    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET)
        return false;

    SOCKADDR_IN addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_listen, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
        return false;

    if (listen(m_listen, MAX_PLAYERS) == SOCKET_ERROR)
        return false;

    printf("[SERVER] Listening on port %d ...\n", port);
    return true;
}

bool ServerSystem::AcceptClient()
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (m_clients[i] == INVALID_SOCKET)
        {
            SOCKET c = accept(m_listen, NULL, NULL);
            if (c == INVALID_SOCKET)
                return false;

            EnterCriticalSection(&m_cs);
            m_clients[i] = c;
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



#pragma region Recv Thread

void ServerSystem::StartRecvThread(int client_id)
{
    std::thread([this, client_id]
    {
        while (m_clients[client_id] != INVALID_SOCKET)
        {
            if (!DoRecv(client_id))
                break;
        }

        printf("[SERVER] Recv thread %d ended.\n", client_id);

    }).detach();
}

bool ServerSystem::DoRecv(int client_id)
{
    char buf[4096];
    int len = recv(m_clients[client_id], buf, sizeof(buf), 0);

    if (len <= 0)
        return false;

    ProcessPacket(buf, client_id);
    return true;
}

#pragma endregion



#pragma region Packet Processing

void ServerSystem::ProcessPacket(char* packet, int client_id)
{
    BasePacket* base = (BasePacket*)packet;

    switch (base->type)
    {
        case CS_UPLOAD_MAP:
        {
            auto* p = (CS_UploadMapPacket*)packet;
            p->Decode();
            HandleMapUpload(p, client_id);
            break;
        }

        case CS_START_SESSION_REQ:
        {
            HandleStartSessionRequest(client_id);
            break;
        }

        case CS_END_SESSION_REQ:
        {
            auto* p = (CS_EndSessionRequestPacket*)packet;
            p->Decode();
            HandleEndSessionRequest(p, client_id);
            break;
        }

        default:
            printf("[Warning] Unknown packet type %d\n", base->type);
            break;
    }
}

#pragma endregion



#pragma region Map Upload / Session Start

void ServerSystem::HandleMapUpload(CS_UploadMapPacket* packet, int client_id)
{
    server_map = packet->UploadMap;

    SendMapUploadResponsePacket(client_id, true);
    BroadcastMapInfo();
}

void ServerSystem::HandleStartSessionRequest(int client_id)
{
    // 기본 맵 0번
    LoadDefaultMap(0);

    BroadcastMapInfo();
}

void ServerSystem::BroadcastMapInfo()
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (m_clients[i] != INVALID_SOCKET)
        {
            SC_MapInfoPacket info;
            info.Init(server_map);
            SendMapInfoPacket(i, &info);
        }
    }
}

#pragma endregion


#pragma region Session Management

void ServerSystem::HandleEndSessionRequest(CS_EndSessionRequestPacket* packet, int client_id)
{
    // 패킷에 포함된 ID와 실제 패킷을 받은 클라이언트의 ID가 일치하는지 확인
    if (packet->player_id != client_id) {
        printf("[Warning] Mismatched player ID in session end request. Recv from client %d, but packet says %hu.\n", client_id, packet->player_id);
    }

    printf("[SERVER] Client %d (ID: %hu) requested to end session.\n", client_id, packet->player_id);

    EnterCriticalSection(&m_cs);
    if (m_clients[client_id] != INVALID_SOCKET) {
        closesocket(m_clients[client_id]);
        m_clients[client_id] = INVALID_SOCKET;
        printf("[SERVER] Client %d disconnected and socket closed.\n", client_id);
    }
    LeaveCriticalSection(&m_cs);

    // 해당 클라이언트의 수신 스레드는 DoRecv()가 실패하거나 m_clients[client_id]가 INVALID_SOCKET이 되어 자동으로 종료됩니다.
}

#pragma endregion



#pragma region Send Functions

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
    send(m_clients[client_id], (char*)packet, sizeof(*packet), 0);
    return true;
}

bool ServerSystem::SendAssignIDPacket(int client_id, u_short id)
{
    SC_AssignIDPacket p(id);
    p.Encode();
    send(m_clients[client_id], (char*)&p, sizeof(p), 0);
    return true;
}

bool ServerSystem::SendEventPacket(int client_id, E_EventType event_type)
{
    SC_EventPacket p(event_type);
    p.Encode();
    send(m_clients[client_id], (char*)&p, sizeof(p), 0);
    return true;
}

#pragma endregion



#pragma region Default Map Load

void ServerSystem::LoadDefaultMap(int map_num)
{
    RECT desk_rt = {0, 0, 1920, 1080};
    Player dummy_players[3];

    server_map = init_map(desk_rt, dummy_players, map_num);
}

#pragma endregion
