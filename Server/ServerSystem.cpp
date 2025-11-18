#include "ServerSystem.h"

/*
 * ===========================================================
 *  ServerSystem.cpp
 *  - 클라이언트와의 TCP 통신 기반 게임 서버 구현
 *  - 기능: 접속 수락, 수신 스레드 관리, 패킷 분기 및 응답
 * ===========================================================
 */

#pragma region

// 생성자: 서버 초기화 (소켓 배열, 임계영역, WSAStartup)
ServerSystem::ServerSystem() : m_listen(INVALID_SOCKET) {
    // 클라이언트 소켓 배열 초기화
    for (int i = 0; i < MAX_PLAYERS; ++i)
        m_clients[i] = INVALID_SOCKET;

    // 멀티스레드 환경 보호를 위한 임계영역 초기화
    InitializeCriticalSection(&m_cs);

    // 윈속(WinSock) 라이브러리 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        printf("[Error] WSAStartup() failed\n");
}

// 소멸자: 모든 소켓 종료 및 리소스 해제
ServerSystem::~ServerSystem() {
    // 연결된 모든 클라이언트 소켓 닫기
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_clients[i] != INVALID_SOCKET) {
            closesocket(m_clients[i]);
            m_clients[i] = INVALID_SOCKET;
        }
    }

    // 리스닝 소켓 닫기
    if (m_listen != INVALID_SOCKET) {
        closesocket(m_listen);
        m_listen = INVALID_SOCKET;
    }

    // 윈속 해제
    WSACleanup();

    // 임계영역 해제
    DeleteCriticalSection(&m_cs);
}

#pragma endregion


#pragma region 

// Start(): 서버 리스닝 소켓 생성 및 바인드/리스닝
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

    // 소켓 바인드 (IP+PORT 할당)
    if (bind(m_listen, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[Error] bind() failed\n");
        closesocket(m_listen);
        return false;
    }

    // 리스닝 시작 (클라이언트 접속 대기)
    if (listen(m_listen, MAX_PLAYERS) == SOCKET_ERROR) {
        printf("[Error] listen() failed\n");
        closesocket(m_listen);
        return false;
    }

    printf("[SERVER] Listening on port %d ...\n", port);
    return true;
}

// AcceptClient(): 새 클라이언트 접속 수락
bool ServerSystem::AcceptClient() {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        // 빈 슬롯(연결 안 된 자리) 탐색
        if (m_clients[i] == INVALID_SOCKET) {
            SOCKET client_sock = accept(m_listen, NULL, NULL);
            if (client_sock == INVALID_SOCKET) {
                printf("[Error] accept() failed (%d)\n", WSAGetLastError());
                return false;
            }

            // 클라이언트 소켓 배열 보호
            EnterCriticalSection(&m_cs);
            m_clients[i] = client_sock;
            LeaveCriticalSection(&m_cs);

            printf("[SERVER] Client %d connected.\n", i);

            // 접속 즉시 클라이언트 ID 부여
            SendAssignIDPacket(i, i);

            // 클라이언트 수신 스레드 시작
            StartRecvThread(i);
            return true;
        }
    }
    return false;
}

#pragma endregion


#pragma region 


// StartRecvThread(): 클라이언트별 수신 스레드 생성
void ServerSystem::StartRecvThread(int client_id) {
    std::thread([this, client_id] {
        while (m_clients[client_id] != INVALID_SOCKET) {
            // 클라이언트가 데이터를 보내면 처리
            if (!DoRecv(client_id))
                break;
        }
        printf("[SERVER] Recv thread for client %d ended.\n", client_id);
    }).detach();
}

// DoRecv(): 클라이언트로부터 데이터 수신
bool ServerSystem::DoRecv(int client_id) {
    char recv_buffer[4096];
    int recv_len = recv(m_clients[client_id], recv_buffer, sizeof(recv_buffer), 0);

    if (recv_len == SOCKET_ERROR) {
        printf("[Error] recv() failed: %d\n", WSAGetLastError());
        return false;
    }
    if (recv_len == 0) {
        // 클라이언트가 정상적으로 연결 종료한 경우
        printf("[SERVER] Client %d disconnected.\n", client_id);
        closesocket(m_clients[client_id]);
        m_clients[client_id] = INVALID_SOCKET;
        return false;
    }

    // 수신된 버퍼를 패킷 단위로 처리
    ProcessPacket(recv_buffer, client_id);
    return true;
}

// ProcessPacket(): 패킷 헤더(type)에 따라 분기 처리
void ServerSystem::ProcessPacket(char* packet, int client_id) {
    BasePacket* base_p = (BasePacket*)packet;
    printf("[SERVER] Received packet type: %d from client %d\n", base_p->type, client_id);

    switch (base_p->type) {
    case CS_UPLOAD_MAP: {
        // 클라이언트가 맵 업로드 요청 시
        auto* map_packet = (CS_UploadMapPacket*)packet;
        map_packet->Decode();
        map_packet->Log();
        HandleMapUpload(map_packet, client_id);
        break;
    }
    case CS_START_SESSION_REQ: {
        // 게임 시작 요청
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

// HandleMapUpload()
// 클라이언트가 전송한 맵 데이터를 처리하고 성공 응답 전송
void ServerSystem::HandleMapUpload(CS_UploadMapPacket* packet, int client_id) {
    printf("[SERVER] Handling CS_UploadMapPacket from client %d\n", client_id);

    // (TODO) 실제 맵 데이터 검증 로직 추가 가능
    bool success = true;

    // 업로드 결과 응답 패킷 전송
    SendMapUploadResponsePacket(client_id, success);
}

// SendMapUploadResponsePacket()
// 맵 업로드 성공 여부를 클라이언트에게 전달
bool ServerSystem::SendMapUploadResponsePacket(int client_id, bool is_success) {
    SC_MapUploadResponsePacket rsp(is_success);
    rsp.Encode();

    int sent = send(m_clients[client_id], (char*)&rsp, sizeof(rsp), 0);
    if (sent == SOCKET_ERROR) {
        printf("[Error] send() failed: %d\n", WSAGetLastError());
        return false;
    }

    printf("[SERVER] Sent SC_MapUploadResponsePacket (%d bytes)\n", sent);
    rsp.Log();
    return true;
}

// HandleStartSessionRequest()
// 클라이언트가 게임 시작 요청 시 서버가 맵 정보 전달
void ServerSystem::HandleStartSessionRequest(CS_StartSessionRequestPacket* packet, int client_id) {
    printf("[SERVER] Handling CS_StartSessionRequestPacket from client %d\n", client_id);

    // 예시용 맵 정보 패킷 구성
    SC_MapInfoPacket info;
    info.block_count = 2;
    info.blocks[0].x = 100;
    info.blocks[0].y = 200;
    info.blocks[0].Block_rt = { 100, 200, 132, 232 };
    info.blocks[1].x = 300;
    info.blocks[1].y = 400;
    info.blocks[1].Block_rt = { 300, 400, 332, 432 };

    info.object_count = 1;
    info.objects[0].x = 250;
    info.objects[0].y = 260;
    info.objects[0].obj_type = Object_type::Spike;
    info.objects[0].Obj_rt = { 250, 260, 280, 290 };

    info.enemy_spawn_count = 1;
    info.enemy_spawns[0].x = 600;
    info.enemy_spawns[0].y = 450;

    info.player_start_pos[0] = { 100, 100 };
    info.player_start_pos[1] = { 150, 100 };
    info.player_start_pos[2] = { 200, 100 };

    // 모든 클라이언트에게 맵 정보 전송
    SendMapInfoPacket(client_id, &info);
}


// SendMapInfoPacket()
// 게임 맵 데이터(블록, 오브젝트, 스폰 정보)를 전송
bool ServerSystem::SendMapInfoPacket(int client_id, SC_MapInfoPacket* packet) {
    packet->Encode();
    int sent = send(m_clients[client_id], (char*)packet, sizeof(*packet), 0);

    if (sent == SOCKET_ERROR) {
        printf("[Error] send() failed (client %d): %d\n", client_id, WSAGetLastError());
        return false;
    }

    printf("[SERVER] Sent SC_MapInfoPacket (%d bytes) to client %d\n", sent, client_id);
    packet->Log();
    return true;
}


// SendAssignIDPacket()
// 클라이언트 접속 시 고유 ID 부여 (예: Player 0, Player 1)
bool ServerSystem::SendAssignIDPacket(int client_id, u_short id) {
    SC_AssignIDPacket assign(id);
    assign.Encode();

    int sent = send(m_clients[client_id], (char*)&assign, sizeof(assign), 0);
    if (sent == SOCKET_ERROR) {
        printf("[Error] send() failed for AssignID: %d\n", WSAGetLastError());
        return false;
    }

    printf("[SERVER] Sent SC_AssignIDPacket (%d bytes) to client %d\n", sent, client_id);
    assign.Log();
    return true;
}

// SendEventPacket()
// 특정 게임 이벤트를 클라이언트에게 전달
bool ServerSystem::SendEventPacket(int client_id, E_EventType event_type) {
    SC_EventPacket event_pkt(event_type);
    event_pkt.Encode();

    int sent = send(m_clients[client_id], (char*)&event_pkt, sizeof(event_pkt), 0);
    if (sent == SOCKET_ERROR) {
        printf("[Error] send() failed for EventPacket: %d\n", WSAGetLastError());
        return false;
    }

    printf("[SERVER] Sent SC_EventPacket (%d bytes) to client %d\n", sent, client_id);
    event_pkt.Log();
    return true;
}

#pragma endregion
