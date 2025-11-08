#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include "Packets.h"
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

#define MAX_PLAYERS 3  // 최대 클라이언트 수

class ServerSystem {
public:
    ServerSystem(); // 생성자: 서버 초기화
    ~ServerSystem(); // 소멸자: 리소스 해제

    bool Start(u_short port); // 서버 시작
    bool AcceptClient(); // 클라이언트 연결 수락
    bool DoRecv(int client_id); // 클라이언트 패킷 수신 처리
    void ProcessPacket(char* packet, int client_id);  // 수신된 패킷 처리
    void HandleMapUpload(CS_UploadMapPacket* packet, int client_id);  // 맵 업로드 패킷 처리
    bool SendMapUploadResponsePacket(int client_id, bool is_success); // 맵 업로드 응답 패킷 전송

private:
    SOCKET m_listen;              // 서버 소켓 (리스닝용)
    SOCKET m_clients[MAX_PLAYERS]; // 클라이언트 소켓 배열 (최대 클라이언트 수)

    CRITICAL_SECTION m_cs; // 임계영역 (클라이언트 소켓 배열 보호)

    void StartRecvThread(int client_id); // 클라이언트 수신 스레드 시작
    static DWORD WINAPI ServerRecvThread(LPVOID lpParam); // 클라이언트 수신 스레드 함수
};
