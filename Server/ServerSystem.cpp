#include "ServerSystem.h"

#pragma region Constructor / Destructor

// 서버 초기화
ServerSystem::ServerSystem() : m_listen(INVALID_SOCKET)
{
    now_map = 0;
    map_type = 0;
    // 모든 클라이언트 소켓 초기화
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        m_clients[i] = INVALID_SOCKET;
        memset(&server_players[i], 0, sizeof(Player)); // 서버 authoritative 플레이어 정보
        player_connected[i] = false;   // 추가: 초기 값 설정
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

    // 서버 재시작 시 포트 점유 방지 
    int reuse = 1;
    setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

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

			// Nagle 알고리즘 비활성화 -> 지연 없는 전송
            int flag = 1;
            setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

			// Keep-Alive 옵션 활성화 -> 연결 유지
            int keepAlive = 1;
            setsockopt(c, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(keepAlive));

            // 임계영역 보호
            EnterCriticalSection(&m_cs);
            m_clients[i] = c;
            player_connected[i] = true;   // 추가: 이 유저가 연결됨
            LeaveCriticalSection(&m_cs);

            printf("[SERVER] Client %d connected.\n", i);

            // 1) 접속한 클라이언트에게 자신 ID 부여
            SendAssignIDPacket(i, i);

            // 2) 새 유저에게 기존 접속 유저 목록 전송
            SendExistingPlayersToNew(i);   // 추가

            // 3) 기존 유저들에게 "i번이 접속함" 알림 전송
            for (int j = 0; j < MAX_PLAYERS; ++j)
            {
                if (j == i) continue;                // 자기 자신 제외
                if (!player_connected[j]) continue;  // 접속 중인 유저만

				SendJoinPlayer(j, i);                // 추가
            }

            // 4) 개별 Recv thread 시작
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
    char buf[12000];
    int len = recv(m_clients[client_id], buf, sizeof(buf), 0);

    if (len <= 0)
    {
        HandleDisconnect(client_id); // 클라이언트 종료 처리
        return false;
    }

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
        HandleStartSessionRequest((CS_StartSessionRequestPacket*)packet, client_id);
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
    server_map[now_map] = packet->UploadMap;
    map_type = 99;
    // 성공 응답 2025.11.30 추가함
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (m_clients[i] != INVALID_SOCKET)
        {
            SendMapUploadResponsePacket(i, true);
        }
    }

    // 모든 클라이언트에게 MAP INFO 전송
   // BroadcastMapInfo();
    // 에딧 맵이랑 기본 맵 한번에 해서 함수 하나로 처리하기 귀찮아졌음.
    // 일단 돌아가게는 두고 리팩토링 필요
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (m_clients[i] != INVALID_SOCKET)
        {
            SC_MapInfoPacket info;
            info.Init(server_map[now_map],99);
            SendMapInfoPacket(i, &info);
        }
    }
    // 게임 루프 시작
    if (!game_loop_running)
        StartGameLoop();
}

// 기본맵 시작 패킷 처리
void ServerSystem::HandleStartSessionRequest(CS_StartSessionRequestPacket* packet, int client_id)
{
    ((CS_StartSessionRequestPacket*)packet)->Decode();
    map_type = packet->map_type;
    // 맵 타입이 기본 맵일 경우에는 맵 정보 다 채우고 종료(그 후에 모든 클라한테 맵 정보 다 전달해주면됨,
    // 한번에 4개 다 보낼지, 깃발 닿아서 이벤트 발생 시에 맵 정보 하나씩 보낼지는 추후 고려
    if (map_type != 0) {
        return;
    }
    else {
        Make_Defalt_Map();
        now_map = 0;
    }
    BroadcastMapInfo(); // 모든 클라에게 전송

    if (!game_loop_running)
        StartGameLoop();
}

// MAP INFO 방송 (모든 클라에게 보냄)
// 원래는 에딧 / 기본 공용인데 현재는 기본 맵에만 사용하는 코드가 됨.
void ServerSystem::BroadcastMapInfo()
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        for (int j = 0; j < 4; ++j) {
            if (m_clients[i] != INVALID_SOCKET)
            {
                SC_MapInfoPacket info;
                info.Init(server_map[j], (char)j);
                SendMapInfoPacket(i, &info);
            }
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

    // HandleDisconnect 함수가 모든 정리 작업을 수행하므로 여기서는 호출만 합니다.
    HandleDisconnect(client_id);
}

#pragma endregion



#pragma region Player Update

// 서버 authoritative 플레이어 정보 갱신
void ServerSystem::HandlePlayerUpdate(CS_PlayerUpdatePacket* packet, int client_id)
{
    Player& p = server_players[client_id];

    p.x = packet->pos.x;
    p.y = packet->pos.y;
    p.Walk_state = packet->walk_state;
    p.jump_count = packet->jump_state;
    p.frame_counter = packet->frame_counter;
    p.player_life = packet->life;
    p.DOWN = packet->down;


    if (packet->dir == Direction::LEFT)
    {
        p.LEFT = true;
        p.RIGHT = false;
    }
    else
    {
        p.LEFT = false;
        p.RIGHT = true;
    }

    p.player_rt = { p.x - Size, p.y - Size, p.x + Size, p.y + Size };

    p.window_move = packet->p_window_mv;
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
    //for (int i = 0; i < MAX_PLAYERS; i++)
    //{
    //    if (m_clients[i] != INVALID_SOCKET)
    //    {
    //        Player_Move(&server_players[i], desk_rt);
    //    }
    //}

    // 적 이동 처리
    for (int i = 0; i < server_map[now_map].enemy_count; i++)
    {
        Move_Enemy(&server_map[now_map].enemys[i],
            server_map[now_map].blocks[server_map[now_map].enemys[i].on_block],
            4);

        Update_Enemy_rect(&server_map[now_map].enemys[i]);
    }
}

// 충돌 처리 (Spike / Flag)
void ServerSystem::CheckAllCollisions()
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (m_clients[i] == INVALID_SOCKET) continue;
        RECT dummy;

        for (int o = 0; o < server_map[now_map].object_count; o++)
        {
            if (IntersectRect(&dummy, &server_players[i].player_rt, &server_map[now_map].objects[o].Obj_rt))
            {
                if (server_map[now_map].objects[o].obj_type == Spike)
                    server_players[i].player_life--;

                else if (server_map[now_map].objects[o].obj_type == Flag) {
                    // Broadcast STAGE_CLEAR to ALL connected clients
                    // 만약 일반 맵이라면, STAGE CLEAR, 에딧 맵이면 GAME WIN을 보낸다.
                    // 보스 맵 다루는 거에서 보스 죽으면 GAME_WIN 보내주세요.
                    if (map_type != 0) {
                        for (int j = 0; j < MAX_PLAYERS; ++j) {
                            if (m_clients[j] != INVALID_SOCKET) {
                                SendEventPacket(j, GAME_WIN);
                            }
                        }
                    }
                    else if (map_type == 0 && now_map < 3) {
                        for (int j = 0; j < MAX_PLAYERS; ++j) {
                            if (m_clients[j] != INVALID_SOCKET) {
                                SendEventPacket(j, STAGE_CLEAR);
                            }
                        }
                    }
                    // 기본 맵일 경우에는 서버에서 관리하는 현재 맵 정보도 하나 갱신
                    if (map_type == 0 && now_map < 3) {
                        ++now_map;
                    }
                    return; // Stop processing collisions for this frame to prevent checking against new map with old coords
                }

            }
        }

        for (int j = 0; j < server_map[now_map].enemy_count; j++)
        {
            if(server_map[now_map].enemys[j].is_alive){
                if (IntersectRect(&dummy, &server_players[i].player_rt, &server_map[now_map].enemys[j].enemy_rect))
                {
                    if (server_players[i].DOWN)
                    {
                        server_map[now_map].enemys[j].is_alive = false;
                    }
                    else
                    {
                        SendEventPacket(i, DIE);
                    }
                }
            }
        }
    }
}

#pragma endregion

#pragma region Send Functions

// 업로드 성공/실패 응답 보내기
bool ServerSystem::SendMapUploadResponsePacket(int client_id, bool success)
{
    SC_MapUploadResponsePacket p(success);
    p.Encode();
    send(m_clients[client_id], (char*)&p, sizeof(p), 0);
    return true;
}

// 맵 정보(Map 전체)를 특정 클라이언트에게 전송
bool ServerSystem::SendMapInfoPacket(int client_id, SC_MapInfoPacket* packet)
{
    packet->Encode();
    send(m_clients[client_id], (char*)packet, sizeof(*packet), 0);
    return true;
}

// 클라이언트에게 고유 ID(0~2) 부여
bool ServerSystem::SendAssignIDPacket(int client_id, u_short id)
{
    SC_AssignIDPacket p(id);
    p.Encode();
    send(m_clients[client_id], (char*)&p, sizeof(p), 0);
    return true;
}

// 이벤트 패킷 전송
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

        
        if (src.LEFT)
        {
            p.players[i].dir = Direction::LEFT;
        }
        else {
            p.players[i].dir = Direction::RIGHT;
        }

        p.players[i].window_move = src.window_move;
    }

   
    // Enemy 상태 복사
    Map& curmap = server_map[now_map];

    for (int e = 0; e < curmap.enemy_count; e++)
    {
        Enemy& src = curmap.enemys[e];

        p.enemies[e].is_alive = src.is_alive;
        p.enemies[e].pos.x = src.x;
        p.enemies[e].pos.y = src.y;
        p.enemies[e].dir = src.direction;
        p.enemies[e].move_state = src.move_state;
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

#pragma region Disconnect Handling

// 클라이언트 연결 종료 처리
void ServerSystem::HandleDisconnect(int client_id)
{
    EnterCriticalSection(&m_cs); // 임계영역 시작

    // 1. 서버 로그에 기록
    printf("[SERVER] 클라이언트 ID %d의 연결이 종료되었습니다.\n", client_id);

    // 2. 소켓 정리 및 배열에서 제거
    if (m_clients[client_id] != INVALID_SOCKET)
    {
        closesocket(m_clients[client_id]);
        m_clients[client_id] = INVALID_SOCKET;
    }

    // 3. 게임 상태 업데이트 (GameManager에 반영)
    server_players[client_id].is_connected = false;

    LeaveCriticalSection(&m_cs); // 임계영역 종료
    player_connected[client_id] = false;   // ★ 추가


    // 4. 다른 모든 클라이언트에게 접속 종료 사실을 알림 (패킷 전송)
    SC_DisconnectPacket packet(client_id);
    packet.Encode();

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        // 자기 자신과 연결되지 않은 클라이언트와 유효하지 않은 소켓은 제외하고 모든 클라이언트에게 전송
        if (i == client_id || m_clients[i] == INVALID_SOCKET) {
            continue;
        }
        send(m_clients[i], (char*)&packet, sizeof(packet), 0);
    }
}

#pragma endregion

// 디폴트 맵 생성
void ServerSystem::Make_Defalt_Map()
{
    Map new_map;
    RECT Desk_rect{ 0,0,1920,1080 };
    int randomnum;
    //맵 초기화 및 생성
    memset(&new_map, 0, sizeof(Map));
    //맵 블럭 초기화
    //0번 맵
    new_map.blocks[new_map.block_count].x = Desk_rect.right / 2;
    new_map.blocks[new_map.block_count].y = (Desk_rect.bottom - 100);
    new_map.blocks[new_map.block_count].Block_rt = { Desk_rect.left,Desk_rect.bottom - 128,Desk_rect.right,Desk_rect.bottom };
    new_map.block_count++;

    new_map.blocks[new_map.block_count].x = Desk_rect.left + 448;
    new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 96;
    new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 128,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 128,new_map.blocks[new_map.block_count].y + 32 };
    new_map.block_count++;

    for (int i = 0; i < (Desk_rect.right - 896) / 384; i++)
    {
        randomnum = rand() % 2;
        if (randomnum == 0)
        {
            new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 384;
            new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 192;
            new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 128,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 128,new_map.blocks[new_map.block_count].y + 32 };
            new_map.block_count++;
        }
        else
        {
            new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 384;
            new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 96;
            new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 128,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 128,new_map.blocks[new_map.block_count].y + 32 };
            new_map.block_count++;
        }
    }

    //맵 오브젝트 초기화
    new_map.objects[new_map.object_count].x = Desk_rect.left + 320;
    new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
    new_map.objects[new_map.object_count].obj_type = Spike;
    new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
    new_map.object_count++;

    for (int i = 0; i < (Desk_rect.right - 640) / (Size * 2); i++)
    {
        new_map.objects[new_map.object_count].x = new_map.objects[new_map.object_count - 1].x + (Size * 2);
        new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
        new_map.objects[new_map.object_count].obj_type = Spike;
        new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
        new_map.object_count++;
    }

    new_map.objects[new_map.object_count].x = new_map.objects[new_map.object_count - 1].x + 48;
    new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
    new_map.objects[new_map.object_count].obj_type = Flag;
    new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
    new_map.object_count++;

    //플레이어 생성
    new_map.P_Start_Loc[0].x = Desk_rect.left + 64;
    new_map.P_Start_Loc[0].y = Desk_rect.bottom - 128;
    server_map[0] = new_map;
    //1번 맵

    //맵 초기화 및 생성
    //맵 블럭 초기화
    memset(&new_map, 0, sizeof(Map));

    new_map.blocks[new_map.block_count].x = Desk_rect.right / 2;
    new_map.blocks[new_map.block_count].y = (Desk_rect.bottom - 100);
    new_map.blocks[new_map.block_count].Block_rt = { Desk_rect.left,Desk_rect.bottom - 128,Desk_rect.right,Desk_rect.bottom };
    new_map.block_count++;

    new_map.blocks[new_map.block_count].x = Desk_rect.left + 448;
    new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 96;
    new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 64,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 64,new_map.blocks[new_map.block_count].y + 32 };
    new_map.block_count++;

    for (int i = 0; i < (Desk_rect.bottom - 192) / 240; i++)
    {
        new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 320;
        new_map.blocks[new_map.block_count].y = new_map.blocks[new_map.block_count - 1].y - 192;
        new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 64,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 64,new_map.blocks[new_map.block_count].y + 32 };
        new_map.block_count++;
    }

    //맵 오브젝트 초기화
    new_map.objects[new_map.object_count].x = Desk_rect.left + 320;
    new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
    new_map.objects[new_map.object_count].obj_type = Spike;
    new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
    new_map.object_count++;

    for (int i = 0; i < (Desk_rect.right - 640) / (Size * 2); i++)
    {
        new_map.objects[new_map.object_count].x = new_map.objects[new_map.object_count - 1].x + (Size * 2);
        new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
        new_map.objects[new_map.object_count].obj_type = Spike;
        new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
        new_map.object_count++;
    }

    new_map.objects[new_map.object_count].x = new_map.blocks[new_map.block_count - 1].Block_rt.right - Size;
    new_map.objects[new_map.object_count].y = new_map.blocks[new_map.block_count - 1].Block_rt.top - 8;
    new_map.objects[new_map.object_count].obj_type = Flag;
    new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
    new_map.object_count++;

    //플레이어 생성
    new_map.P_Start_Loc[0].x = Desk_rect.left + 64;
    new_map.P_Start_Loc[0].y = Desk_rect.bottom - 128;

    server_map[1] = new_map;

    //2번 맵
    //맵 초기화 및 생성
    //맵 블럭 초기화
    memset(&new_map, 0, sizeof(Map));

    new_map.blocks[new_map.block_count].x = Desk_rect.right / 2;
    new_map.blocks[new_map.block_count].y = (Desk_rect.bottom - 100);
    new_map.blocks[new_map.block_count].Block_rt = { Desk_rect.left,Desk_rect.bottom - 128,Desk_rect.right,Desk_rect.bottom };
    new_map.block_count++;

    new_map.blocks[new_map.block_count].x = Desk_rect.left + 448;
    new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 96;
    new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 64,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 64,new_map.blocks[new_map.block_count].y + 32 };
    new_map.block_count++;

    for (int i = 0; i < (Desk_rect.bottom - 192) / 240; i++)
    {
        new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 320;
        new_map.blocks[new_map.block_count].y = new_map.blocks[new_map.block_count - 1].y + (rand() % 640 - 320);
        while (new_map.blocks[new_map.block_count].y >= new_map.blocks[0].Block_rt.top)
        {
            new_map.blocks[new_map.block_count].y = new_map.blocks[new_map.block_count - 1].y + (rand() % 640 - 320);
        }
        new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 64,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 64,new_map.blocks[new_map.block_count].y + 32 };
        new_map.block_count++;

        new_map.enemys[new_map.enemy_count] = Make_Enemy(new_map.blocks[new_map.block_count - 1].x, new_map.blocks[new_map.block_count - 1].Block_rt.top - Size, new_map.block_count - 1);
        new_map.enemy_count++;
    }

    //맵 오브젝트 초기화
    new_map.objects[new_map.object_count].x = Desk_rect.left + 320;
    new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
    new_map.objects[new_map.object_count].obj_type = Spike;
    new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
    new_map.object_count++;

    for (int i = 0; i < (Desk_rect.right - 640) / (Size * 2); i++)
    {
        new_map.objects[new_map.object_count].x = new_map.objects[new_map.object_count - 1].x + (Size * 2);
        new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
        new_map.objects[new_map.object_count].obj_type = Spike;
        new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
        new_map.object_count++;
    }

    new_map.objects[new_map.object_count].x = new_map.blocks[new_map.block_count - 1].Block_rt.right - Size;
    new_map.objects[new_map.object_count].y = new_map.blocks[new_map.block_count - 1].Block_rt.top - 8;
    new_map.objects[new_map.object_count].obj_type = Flag;
    new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
    new_map.object_count++;

    //플레이어 생성
    new_map.P_Start_Loc[0].x = Desk_rect.left + 64;
    new_map.P_Start_Loc[0].y = Desk_rect.bottom - 128;

    server_map[2] = new_map;
    //3번 맵(보스전)
    //맵 초기화 및 생성
    //맵 블럭 초기화
    memset(&new_map, 0, sizeof(Map));

    new_map.blocks[new_map.block_count].x = Desk_rect.left + 100;
    new_map.blocks[new_map.block_count].y = Desk_rect.bottom - 200;
    new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 160,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 160,new_map.blocks[new_map.block_count].y + 32 };
    new_map.block_count++;

    for (int i = 0; i < ((Desk_rect.right - 260) / 480); i++)
    {
        new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 480;
        new_map.blocks[new_map.block_count].y = Desk_rect.bottom - ((rand() % 256) + 160);
        new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 160,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 160,new_map.blocks[new_map.block_count].y + 32 };
        new_map.block_count++;
    }

    new_map.enemys[new_map.enemy_count] = Make_Enemy(new_map.blocks[1].x, new_map.blocks[1].Block_rt.top - Size, 1);
    new_map.enemy_count++;

    //보스 초기화
    new_map.boss.x = Desk_rect.right - 160;
    new_map.boss.y = Desk_rect.bottom - 320;
    new_map.boss.life = 3;
    new_map.boss.boss_rect = { new_map.boss.x - 160,new_map.boss.y - 160,new_map.boss.x + 160,new_map.boss.y + 160 };
    new_map.boss_count++;
    new_map.boss.attack_time = 480;

    //플레이어 생성
    new_map.P_Start_Loc[0].x = new_map.blocks[0].x;
    new_map.P_Start_Loc[0].y = new_map.blocks[0].Block_rt.top - Size;

    server_map[3] = new_map;
}

#pragma endregion



#pragma region Default Map Load

// 디폴트 맵 로드
void ServerSystem::LoadDefaultMap(int map_num)
{
    RECT desk_rt = { 0,0,1920,1080 };

    Player dummy[3];
    // server_map = init_map(desk_rt, dummy, map_num);
    Make_Defalt_Map();
    // 모든 플레이어 동일 스폰 (추후 분리 가능)
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        server_players[i] =
            Make_Player(server_map[0].P_Start_Loc[0].x, server_map[0].P_Start_Loc[0].y);
    }
}

#pragma endregion

// 특정 클라이언트에게 다른 플레이어 접속 알림 전송
bool ServerSystem::SendJoinPlayer(int to_client, int joined_id)
{
    // 패킷 생성
    SC_PlayerJoinPacket packet(joined_id);
    packet.Encode();  // 바이트 변환

    // 해당 클라 소켓으로 전송
    int result = send(m_clients[to_client], (char*)&packet, sizeof(packet), 0);

    // 전송 길이가 정확해야 성공
    return (result == sizeof(packet));
}

// 새로운 클라이언트에게 기존 접속 플레이어들 정보 전송
bool ServerSystem::SendExistingPlayersToNew(int new_client_id)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (i == new_client_id) continue;  // 자기 자신 제외
        if (!player_connected[i]) continue; // 접속한 클라만

        // C가 왔다면 → C에게 A, B 알려주기
        SendJoinPlayer(new_client_id, i);
    }
    return true;
}
