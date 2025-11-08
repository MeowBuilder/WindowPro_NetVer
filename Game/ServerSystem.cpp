#include "ServerSystem.h"

ServerSystem::ServerSystem() : m_listen(INVALID_SOCKET) {
    // 서버 소켓 및 클라이언트 소켓 초기화
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_clients[i] = INVALID_SOCKET;
    }

    // 임계 영역 초기화
    InitializeCriticalSection(&m_cs);

    // Winsock 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[Error] WSAStartup() failed\n");
    }
}

ServerSystem::~ServerSystem() {
    // 클라이언트 연결 종료 및 소켓 닫기
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_clients[i] != INVALID_SOCKET) {
            closesocket(m_clients[i]);
            m_clients[i] = INVALID_SOCKET;
        }
    }

    // 리스닝 소켓 종료
    if (m_listen != INVALID_SOCKET) {
        closesocket(m_listen);
        m_listen = INVALID_SOCKET;
    }

    WSACleanup();

    // 임계 영역 해제
    DeleteCriticalSection(&m_cs);
}

bool ServerSystem::Start(u_short port) {
    // 리스닝 소켓 생성
    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET) {
        printf("[Error] socket() failed\n");
        return false;
    }

    // 소켓에 주소 설정
    SOCKADDR_IN addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);  // 포트 설정 (예시: 9000번 포트)
    addr.sin_addr.s_addr = INADDR_ANY;

    // 소켓 바인딩
    if (bind(m_listen, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[Error] bind() failed\n");
        closesocket(m_listen);
        return false;
    }

    // 클라이언트 접속 대기
    if (listen(m_listen, MAX_PLAYERS) == SOCKET_ERROR) {
        printf("[Error] listen() failed\n");
        closesocket(m_listen);
        return false;
    }

    printf("[Info] Server started, waiting for clients...\n");

    return true;
}

bool ServerSystem::AcceptClient() {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_clients[i] == INVALID_SOCKET) {
            SOCKET client_sock = accept(m_listen, NULL, NULL);
            if (client_sock == INVALID_SOCKET) {
                printf("[Error] accept() failed\n");
                return false;
            }

            // 새 클라이언트 연결 수락
            EnterCriticalSection(&m_cs);  // 임계영역 시작
            m_clients[i] = client_sock;
            LeaveCriticalSection(&m_cs);  // 임계영역 끝

            printf("[Info] Client %d connected.\n", i);

            // 클라이언트 수신 스레드 시작
            StartRecvThread(i);
            return true;
        }
    }

    return false;  // 모든 클라이언트 슬롯이 가득 찼으면 false 반환
}

void ServerSystem::StartRecvThread(int client_id) {
    // 클라이언트 수신 스레드 시작
    std::thread([this, client_id] {
        while (m_clients[client_id] != INVALID_SOCKET) {
            DoRecv(client_id);
        }
        }).detach();
}

bool ServerSystem::DoRecv(int client_id) {
    char recv_buffer[4096];
    int recv_len = recv(m_clients[client_id], recv_buffer, sizeof(recv_buffer), 0);

    if (recv_len == SOCKET_ERROR) {
        printf("[Error] recv() failed with error: %d\n", WSAGetLastError());
        return false;
    }

    if (recv_len == 0) {
        printf("[Info] Client %d disconnected.\n", client_id);
        return false;
    }

    printf("[Debug] Received %d bytes from client %d.\n", recv_len, client_id);
    ProcessPacket(recv_buffer, client_id);
    return true;
}

void ServerSystem::ProcessPacket(char* packet, int client_id) {
    BasePacket* base_p = (BasePacket*)packet;

    printf("[Debug] Processing packet type: %d\n", base_p->type);

    switch (base_p->type) {
    case CS_UPLOAD_MAP: {
        CS_UploadMapPacket* map_packet = (CS_UploadMapPacket*)packet;
        HandleMapUpload(map_packet, client_id);  // 맵 업로드 처리
        break;
    }
    default: {
        printf("[Warning] Received unknown packet type: %d\n", base_p->type);
        break;
    }
    }
}

void ServerSystem::HandleMapUpload(CS_UploadMapPacket* packet, int client_id) {
    printf("[Info] Handling map upload for client %d\n", client_id);

    // 맵 업로드 결과 응답 전송
    bool success = true;  // 임시 성공 값 (추후 실제 검증 필요)
    SendMapUploadResponsePacket(client_id, success);
}

bool ServerSystem::SendMapUploadResponsePacket(int client_id, bool is_success) {
    SC_MapUploadResponsePacket response;
    response.is_success = is_success;

    // 패킷을 인코딩하여 전송
    response.Encode();
    int send_len = send(m_clients[client_id], (char*)&response, sizeof(response), 0);

    if (send_len == SOCKET_ERROR) {
        printf("[Error] send() failed with error: %d\n", WSAGetLastError());
        return false;
    }

    printf("[Info] Sent map upload response to client %d\n", client_id);
    return true;
}
