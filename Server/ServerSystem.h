#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <thread>
#include <atomic>
#include "Packets.h"
#include "GameManager.h"   // init_map() 사용을 위해 추가


#pragma comment(lib, "ws2_32.lib")

#define MAX_PLAYERS 3  // 최대 클라이언트 수

class ServerSystem {
public:
    ServerSystem();   // 서버 초기화 (소켓/WSAStartup)
    ~ServerSystem();  // 자원 해제 및 소켓 종료

    // 서버 기본 기능
    bool Start(u_short port);     // 서버 소켓 생성 및 리스닝 시작
    bool AcceptClient();          // 클라이언트 접속 수락
    bool DoRecv(int client_id);   // 클라이언트 패킷 수신
    void ProcessPacket(char* packet, int client_id); // 패킷 타입 분기 처리

    // ==============================
    // 패킷 처리 핸들러
    // ==============================
    void HandleMapUpload(CS_UploadMapPacket* packet, int client_id);      // 맵 업로드 처리
    bool SendMapUploadResponsePacket(int client_id, bool is_success);     // 업로드 응답 전송

    void HandleStartSessionRequest(CS_StartSessionRequestPacket* packet, int client_id); // 세션 시작 요청 처리
    bool SendMapInfoPacket(int client_id, SC_MapInfoPacket* packet);      // 맵 정보 전송

    bool SendAssignIDPacket(int client_id, u_short id);                   // 클라이언트 ID 할당 패킷 전송
    bool SendEventPacket(int client_id, E_EventType event_type);          // 특정 이벤트 패킷 전송

private:
    // 내부 관리용
    SOCKET m_listen;                      // 리스닝 소켓
    SOCKET m_clients[MAX_PLAYERS];        // 클라이언트 소켓 배열
    CRITICAL_SECTION m_cs;                // 소켓 배열 보호용 임계영역

    void StartRecvThread(int client_id);  // 수신 스레드 시작

    //  추가된 멤버
    Map server_map;         // 서버에서 유지하는 최종 맵
    void LoadDefaultMap(int map_num); // init_map() 호출용
};
