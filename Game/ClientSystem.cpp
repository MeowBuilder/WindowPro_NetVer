#include "ClientSystem.h"
#include <cstdio>
#include "resource_ip_dialog.h" // WM_USER_UPDATE_PLAYER_LIST를 사용하기 위해 추가

ClientSystem::ClientSystem() : sock(INVALID_SOCKET), hRecvThread(NULL), my_player_id(-1) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[Error] WSAStartup() failed\n");
    }
    Mapready = false;
    StartGame = false;
    current_map_index = 0;
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


bool ClientSystem::DoRecv()
{
    char buf[4096];

    int recv_len = recv(sock, buf, sizeof(buf), 0);
    if (recv_len == SOCKET_ERROR)
    {
        printf("[Error] recv() failed: %d\n", WSAGetLastError());
        Disconnect();
        return false;
    }
    if (recv_len == 0)
    {
        printf("[Info] server closed connection.\n");
        Disconnect();
        return false;
    }

    // 1) 새로 받은 데이터 스트림 버퍼에 붙이기
    if (m_streamLen + recv_len > RECV_STREAM_BUFFER_SIZE)
    {
        printf("[Error] stream buffer overflow. drop buffer.\n");
        m_streamLen = 0; // 안전하게 비워버림
        return false;
    }

    memcpy(m_streamBuf + m_streamLen, buf, recv_len);
    m_streamLen += recv_len;

    // 2) 스트림 버퍼에서 패킷 단위로 잘라 처리
    int offset = 0;

    while (true)
    {
        // 최소한 헤더(BasePacket) 만큼은 있어야 패킷 크기를 알 수 있음
        if (m_streamLen - offset < (int)sizeof(BasePacket))
        {
            break;
        }

        BasePacket* header = (BasePacket*)(m_streamBuf + offset);

        // 아직 네트워크 바이트 순서이므로 ntohs로 실제 크기 확인
        uint16_t packetSize = ntohs(header->size);

        if (packetSize == 0)
        {
            printf("[Error] packetSize == 0. drop stream.\n");
            m_streamLen = 0;
            return false;
        }
        if (packetSize > RECV_STREAM_BUFFER_SIZE)
        {
            printf("[Error] packetSize too big: %hu\n", packetSize);
            m_streamLen = 0;
            return false;
        }

        // 패킷 전체가 다 도착했는지 확인
        if (m_streamLen - offset < (int)packetSize)
        {
            // 아직 덜 옴 → 다음 recv까지 기다림
            break;
        }

        // 여기서부터는 m_streamBuf + offset 에 packetSize 바이트짜리
        // "완성된 패킷"이 들어있음.
        char packetBuf[RECV_STREAM_BUFFER_SIZE];
        memcpy(packetBuf, m_streamBuf + offset, packetSize);

        // 실제 패킷 처리
        ProcessPacket(packetBuf);

        offset += packetSize;
    }

    // 3) 아직 다 못 쓴(불완전한) 조각이 남았으면 앞으로 당김
    if (offset > 0)
    {
        int remain = m_streamLen - offset;
        if (remain > 0)
        {
            memmove(m_streamBuf, m_streamBuf + offset, remain);
        }
        m_streamLen = remain;
    }

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
        case SC_GAME_STATE: {
            SC_GameStatePacket* p = (SC_GameStatePacket*)packet_buf;
            p->Decode(); // 네트워크 바이트 순서에서 호스트 바이트 순서로 변환
            //p->Log();    // 너무 자주 호출되므로 주석 처리
            HandleGameState(p);
            break;
        }
        case SC_DISCONNECT: {
            SC_DisconnectPacket* p = (SC_DisconnectPacket*)packet_buf;
            p->Decode();
            // p->Log(); // 디버깅용, 필요시 활성화

            if (p->disconnected_player_id >= 0 && p->disconnected_player_id < 3) {
                players[p->disconnected_player_id].is_connected = false;
                printf("[Info] Player %hu disconnected.\n", p->disconnected_player_id);
                // 다이얼로그가 열려 있다면 업데이트 메시지 전송
                if (g_hIpInputDlg && IsWindow(g_hIpInputDlg)) {
                    PostMessage(g_hIpInputDlg, WM_USER_UPDATE_PLAYER_LIST, 0, 0);
                }
            } else {
                printf("[Warning] Received SC_DISCONNECT with invalid player ID: %hu\n", p->disconnected_player_id);
            }
            break;
        }
        case SC_MAP_UPLOAD_RSP: {
            SC_MapUploadResponsePacket* p = (SC_MapUploadResponsePacket*)packet_buf;
            p->Decode();
            // p->Log(); // 디버깅용, 필요시 활성화
            HandleMapUploadResponse(p);
            break;
        }
        case SC_PLAYER_JOIN: { // 새로 추가된 SC_PLAYER_JOIN 패킷 처리
            SC_PlayerJoinPacket* p = (SC_PlayerJoinPacket*)packet_buf;
            p->Decode();
            p->Log(); // 디버깅용
            HandlePlayerJoin(p);
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
    
    // 내 플레이어의 연결 상태를 true로 설정
    if (my_player_id >= 0 && my_player_id < 3) {
        players[my_player_id].is_connected = true;
    }

    // 내 ID가 할당되면 리스트 박스 업데이트를 위해 다이얼로그에 메시지 전송
    if (g_hIpInputDlg && IsWindow(g_hIpInputDlg)) {
        PostMessage(g_hIpInputDlg, WM_USER_UPDATE_PLAYER_LIST, 0, 0);
    }
}

void ClientSystem::SwitchToNextMap() {
    EnterCriticalSection(&m_map_cs);
    if (current_map_index < 3) {
        current_map_index++;
        printf("[Info] Switched to map index: %d\n", current_map_index);
    }
    LeaveCriticalSection(&m_map_cs);
}

void ClientSystem::HandleEvent(SC_EventPacket* packet) {
    printf("[Info] Handling Event: ");
    switch (packet->event_type) {
        case STAGE_CLEAR:
            printf("STAGE_CLEAR\n");
            SwitchToNextMap();
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

void ClientSystem::HandleGameState(SC_GameStatePacket* packet) {
    // 1. 플레이어 상태 업데이트
    for (int i = 0; i < 3; ++i) {
        // 내 캐릭터는 직접 조작하므로 서버의 정보로 덮어쓰지 않음
        if (i == my_player_id) {
            continue;
        }

        players[i].is_connected = packet->players[i].is_connected;
        if (players[i].is_connected) {
            players[i].x = packet->players[i].pos.x;
            players[i].y = packet->players[i].pos.y;
            players[i].player_life = packet->players[i].life;
            players[i].Walk_state = packet->players[i].walk_state;
            players[i].Jump_state = packet->players[i].jump_state;
            players[i].frame_counter = packet->players[i].frame_counter;
            if (packet->players[i].dir == Direction::LEFT)
            {
                players[i].LEFT = true;
                players[i].RIGHT = false;
            }
            else
            {
                players[i].LEFT = false;
                players[i].RIGHT = true;
            }
            players[i].window_move = packet->players[i].window_move;
        }
    }

    EnterCriticalSection(&m_map_cs);

    // 2. 적 상태 업데이트
    Map& current_map = m_maps[current_map_index];
    for (int i = 0; i < current_map.enemy_count; ++i) {
        current_map.enemys[i].is_alive = packet->enemies[i].is_alive;
        if (current_map.enemys[i].is_alive) {
            current_map.enemys[i].x = packet->enemies[i].pos.x;
            current_map.enemys[i].y = packet->enemies[i].pos.y;
            current_map.enemys[i].direction = packet->enemies[i].dir;
            current_map.enemys[i].move_state = packet->enemies[i].move_state;
        }
    }

    // 3. 보스 상태 업데이트
    if (current_map.boss_count > 0) {
        current_map.boss.x = packet->boss.pos.x;
        current_map.boss.y = packet->boss.pos.y;
        current_map.boss.life = packet->boss.life;
        current_map.boss.attack_time = packet->boss.attack_time;
        // current_map.boss.dir = packet->boss.dir; // boss 구조체에 dir 없음
    }

    LeaveCriticalSection(&m_map_cs);
}

void ClientSystem::HandleMapInfo(SC_MapInfoPacket* packet)
{
    EnterCriticalSection(&m_map_cs);

    if (packet->map_index >= 0 && packet->map_index < 4) {
        m_maps[packet->map_index] = packet->mapInfo;
    }

    LeaveCriticalSection(&m_map_cs);
    
    // Start game if map 0 is received
    if (packet->map_index == 0) {
        Mapready = true;
        StartGame = true;
        SetEvent(maprecvEvent);
    }
}

void ClientSystem::HandleMapUploadResponse(SC_MapUploadResponsePacket* packet)
{
    if (packet->is_success)
    {
        StartGame = true;
    }
}


Map ClientSystem::GetMap(int index)
{
    Map temp_map;

    int target_index = (index == -1) ? current_map_index : index;

    if (!Mapready) {
        WaitForSingleObject(maprecvEvent, INFINITE);
    }

    EnterCriticalSection(&m_map_cs);
    temp_map = m_maps[target_index];

    // Main.cpp에서 P_Start_Loc[my_id]를 사용하므로 여기서 미리 계산해서 채워둠
    temp_map.P_Start_Loc[1].x = temp_map.P_Start_Loc[0].x + 100;
    temp_map.P_Start_Loc[1].y = temp_map.P_Start_Loc[0].y;
    temp_map.P_Start_Loc[2].x = temp_map.P_Start_Loc[0].x + 200;
    temp_map.P_Start_Loc[2].y = temp_map.P_Start_Loc[0].y;

    players[0] = Make_Player(temp_map.P_Start_Loc[0].x, temp_map.P_Start_Loc[0].y);
    players[1] = Make_Player(temp_map.P_Start_Loc[1].x, temp_map.P_Start_Loc[1].y);
    players[2] = Make_Player(temp_map.P_Start_Loc[2].x, temp_map.P_Start_Loc[2].y);
    for (size_t i = 0; i < 3; i++)
    {
        players[i].is_connected = true;
    }

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
    // Reset client-side map state when a new session request is sent
    EnterCriticalSection(&m_map_cs);
    current_map_index = 0; // Always start from map 0 for a new session
    Mapready = false;
    StartGame = false;
    ResetEvent(maprecvEvent); // Reset the event so GetMap will wait for new map info
    LeaveCriticalSection(&m_map_cs);

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

// [S->C] 새로운 플레이어의 접속을 알리는 패킷 처리
void ClientSystem::HandlePlayerJoin(SC_PlayerJoinPacket* packet) {
    u_short joined_player_id = packet->joined_player_id;

    // 유효한 플레이어 ID인지 확인
    if (joined_player_id >= 0 && joined_player_id < 3) {
        // 해당 플레이어의 연결 상태를 true로 설정
        // GameManager.h에 정의된 Player 구조체의 is_connected 필드를 사용
        players[joined_player_id].is_connected = true;
        printf("[Info] Player %hu has joined the game.\n", joined_player_id);

        // 다이얼로그가 열려 있다면 업데이트 메시지 전송
        if (g_hIpInputDlg && IsWindow(g_hIpInputDlg)) {
            PostMessage(g_hIpInputDlg, WM_USER_UPDATE_PLAYER_LIST, 0, 0);
        }
    } else {
        printf("[Warning] Received SC_PLAYER_JOIN with invalid player ID: %hu\n", joined_player_id);
    }
}