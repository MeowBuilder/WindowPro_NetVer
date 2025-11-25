#include "ServerSystem.h"

#pragma region Constructor / Destructor

// 서버 초기화
ServerSystem::ServerSystem() : m_listen(INVALID_SOCKET)
{
    // 모든 클라이언트 소켓 초기화
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        m_clients[i] = INVALID_SOCKET;
        memset(&server_players[i], 0, sizeof(Player)); // 서버 authoritative 플레이어 정보
    }

    // 임계영역 초기화 (멀티스레드 보호)
    InitializeCriticalSection(&m_cs);

    // Winsock 시작
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        printf("[Error] WSAStartup() failed\n");
}

// 서버 종료 처리
ServerSystem::~ServerSystem()
{
    game_loop_running = false; // 게임 루프 중지

    // 모든 클라이언트 소켓 정리
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (m_clients[i] != INVALID_SOCKET)
        {
            closesocket(m_clients[i]);
            m_clients[i] = INVALID_SOCKET;
        }
    }

    // 리슨 소켓 정리
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

// 서버 포트 바인드 + 리슨
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

// 클라이언트 연결 처리
bool ServerSystem::AcceptClient()
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        // 빈 자리 발견
        if (m_clients[i] == INVALID_SOCKET)
        {
            SOCKET c = accept(m_listen, NULL, NULL);
            if (c == INVALID_SOCKET)
                return false;

            // 임계영역 보호
            EnterCriticalSection(&m_cs);
            m_clients[i] = c;
            LeaveCriticalSection(&m_cs);

            printf("[SERVER] Client %d connected.\n", i);

            // 접속한 클라이언트에게 자신 ID 부여
            SendAssignIDPacket(i, i);

            // 개별 Recv thread 시작
            StartRecvThread(i);

            return true;
        }
    }
    return false;
}

#pragma endregion



#pragma region Recv Thread

// 클라이언트마다 별도 수신 스레드 운용
void ServerSystem::StartRecvThread(int client_id)
{
    std::thread([this, client_id]
        {
            while (m_clients[client_id] != INVALID_SOCKET)
            {
                if (!DoRecv(client_id))
                    break; // Recv 실패 → 종료
            }

            printf("[SERVER] Recv thread %d ended.\n", client_id);

        }).detach();
}

// 실제 수신 처리 (recv → ProcessPacket)
bool ServerSystem::DoRecv(int client_id)
{
    char buf[4096];
    int len = recv(m_clients[client_id], buf, sizeof(buf), 0);

    if (len <= 0)
        return false; // 클라이언트 종료

    ProcessPacket(buf, client_id);
    return true;
}

#pragma endregion



#pragma region Packet Processing

// 모든 수신 패킷의 공통 진입점
void ServerSystem::ProcessPacket(char* packet, int client_id)
{
    BasePacket* base = (BasePacket*)packet;

    switch (base->type)
    {
    case CS_UPLOAD_MAP:
        ((CS_UploadMapPacket*)packet)->Decode();
        HandleMapUpload((CS_UploadMapPacket*)packet, client_id);
        break;

    case CS_START_SESSION_REQ:
        HandleStartSessionRequest(client_id);
        break;

    case CS_END_SESSION_REQ:
        ((CS_EndSessionRequestPacket*)packet)->Decode();
        HandleEndSessionRequest((CS_EndSessionRequestPacket*)packet, client_id);
        break;

    case CS_PLAYER_UPDATE:
        ((CS_PlayerUpdatePacket*)packet)->Decode();
        HandlePlayerUpdate((CS_PlayerUpdatePacket*)packet, client_id);
        break;

    default:
        printf("[Warning] Unknown packet type %d\n", base->type);
        break;
    }
}

#pragma endregion



#pragma region Map Upload / Session Start

// 제작맵 업로드 처리
void ServerSystem::HandleMapUpload(CS_UploadMapPacket* packet, int client_id)
{
    // 서버 authoritative 맵 갱신
    server_map = packet->UploadMap;

    // 성공 응답
    SendMapUploadResponsePacket(client_id, true);

    // 모든 클라이언트에게 MAP INFO 전송
    BroadcastMapInfo();

    // 게임 루프 시작
    if (!game_loop_running)
        StartGameLoop();
}

// 기본맵 시작 패킷 처리
void ServerSystem::HandleStartSessionRequest(int client_id)
{
    LoadDefaultMap(0); // 기본맵 로딩

    BroadcastMapInfo(); // 모든 클라에게 전송

    if (!game_loop_running)
        StartGameLoop();
}

// MAP INFO 방송 (모든 클라에게 보냄)
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

// 종료 요청 처리
void ServerSystem::HandleEndSessionRequest(CS_EndSessionRequestPacket* packet, int client_id)
{
    if (packet->player_id != client_id)
        printf("[Warning] Packet ID mismatch.\n");

    EnterCriticalSection(&m_cs);

    if (m_clients[client_id] != INVALID_SOCKET)
    {
        closesocket(m_clients[client_id]);
        m_clients[client_id] = INVALID_SOCKET;
    }

    LeaveCriticalSection(&m_cs);
}

#pragma endregion



#pragma region Player Update

// 서버 authoritative 플레이어 정보 갱신
void ServerSystem::HandlePlayerUpdate(CS_PlayerUpdatePacket* packet, int client_id)
{
    Player& p = server_players[client_id];

    p.x = packet->pos.x;
    p.y = packet->pos.y;
    // 충돌 체크에 필요한 사각형 갱신
    p.player_rt = { p.x - Size, p.y - Size, p.x + Size, p.y + Size };
}

#pragma endregion



#pragma region Game Loop

// 서버 메인 게임 루프 ( authoritative update → broadcast )
void ServerSystem::StartGameLoop()
{
    if (game_loop_running) return;

    game_loop_running = true;

    std::thread([this]
        {
            while (game_loop_running)
            {
                UpdateAllPositions();   // 서버 기준 플레이어/적 이동
                CheckAllCollisions();   // 스파이크/깃발 등 충돌
                BroadcastGameState();   // 모든 클라이언트에게 상태 전송

                Sleep(30); // 33ms → 약 30FPS
            }
        }).detach();

    printf("[SERVER] Game loop started.\n");
}

#pragma endregion



#pragma region Game Logic

// 서버 authoritative 좌표 업데이트
void ServerSystem::UpdateAllPositions()
{
    RECT desk_rt = { 0,0,1920,1080 };

    // 플레이어 서버 authoritative 이동
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (m_clients[i] != INVALID_SOCKET)
        {
            Player_Move(&server_players[i], desk_rt);
        }
    }

    // 적 이동 처리
    for (int i = 0; i < server_map.enemy_count; i++)
    {
        Move_Enemy(&server_map.enemys[i],
            server_map.blocks[server_map.enemys[i].on_block],
            4);

        Update_Enemy_rect(&server_map.enemys[i]);
    }
}

// 충돌 처리 (Spike / Flag)
void ServerSystem::CheckAllCollisions()
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (m_clients[i] == INVALID_SOCKET) continue;

        Player& p = server_players[i];

        for (int o = 0; o < server_map.object_count; o++)
        {
            RECT dummy;
            if (IntersectRect(&dummy, &p.player_rt, &server_map.objects[o].Obj_rt))
            {
                if (server_map.objects[o].obj_type == Spike)
                    p.player_life--;

                else if (server_map.objects[o].obj_type == Flag)
                    SendEventPacket(i, STAGE_CLEAR);
            }
        }
    }
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

// 전 플레이어 상태 방송 (GameStatePacket)
bool ServerSystem::BroadcastGameState()
{
    SC_GameStatePacket p;

    // 서버 authoritative 상태 입력
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        Player& src = server_players[i];

        p.players[i].is_connected = (m_clients[i] != INVALID_SOCKET);
        p.players[i].pos.x = src.x;
        p.players[i].pos.y = src.y;
        p.players[i].life = src.player_life;
        p.players[i].walk_state = src.Walk_state;
        p.players[i].jump_state = src.Jump_state;
        p.players[i].dir = src.LEFT ? LEFT : RIGHT;
    }

    // 클라이언트들에게 전송
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (m_clients[i] != INVALID_SOCKET)
        {
            SC_GameStatePacket sendP = p;
            sendP.Encode();
            send(m_clients[i], (char*)&sendP, sizeof(sendP), 0);
        }
    }
    return true;
}

#pragma endregion



#pragma region Default Map Load

// 기본맵 생성 + 플레이어 스폰 초기화
void ServerSystem::LoadDefaultMap(int map_num)
{
    RECT desk_rt = { 0,0,1920,1080 };

    Player dummy[3];
    server_map = init_map(desk_rt, dummy, map_num);

    // 모든 플레이어 동일 스폰 (추후 분리 가능)
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        server_players[i] =
            Make_Player(server_map.P_Start_Loc[0].x, server_map.P_Start_Loc[0].y);
    }
}

#pragma endregion
