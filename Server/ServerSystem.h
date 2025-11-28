#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <cstdio>

#include "Packets.h"        // 모든 패킷 구조체 정의
#include "GameManager.h"    // Map, Player 구조체 및 게임 로직 함수
#include "Player&Enemy.h"    // 플레이어/적 이동 & 충돌 계산을 담당하는 핵심 로직


#pragma comment(lib, "ws2_32.lib")

// 서버에 허용되는 최대 플레이어 수
#define MAX_PLAYERS 3

//  ServerSystem 클래스 선언부
//  - 서버 전체 네트워크 + 게임 로직 관리
//  - 클라 연결 / 패킷 처리 / 게임 루프 포함
class ServerSystem
{
public:

    // 생성자 / 소멸자
    // - Winsock 초기화
    // - 소켓 초기화
    ServerSystem();
    ~ServerSystem();


    // 서버 실행
    // 서버 시작 (포트 바인드 + 리슨)
    bool Start(u_short port);

    // 새 클라이언트 접속 대기 및 수락
    bool AcceptClient();

    // 특정 클라이언트로부터 recv()
    bool DoRecv(int client_id);

    // recv된 패킷 분기 처리
    void ProcessPacket(char* packet, int client_id);



    // 패킷 처리 함수들
    // 클라이언트가 제작한 Map 업로드 패킷 처리
    void HandleMapUpload(CS_UploadMapPacket* packet, int client_id);

    // 기본 맵을 사용해 게임 세션 시작 요청 처리
    void HandleStartSessionRequest(CS_StartSessionRequestPacket* packet, int client_id);

    // 클라이언트가 "게임 끝" 요청 보낼 때 처리
    void HandleEndSessionRequest(CS_EndSessionRequestPacket* packet, int client_id);

    // 클라이언트의 PlayerUpdate 패킷 처리 (서버 authoritative update)
    void HandlePlayerUpdate(CS_PlayerUpdatePacket* packet, int client_id);


    // 패킷 송신 함수들
    // 업로드 성공/실패 응답 보내기
    bool SendMapUploadResponsePacket(int client_id, bool is_success);

    // 맵 정보(Map 전체)를 특정 클라이언트에게 전송
    bool SendMapInfoPacket(int client_id, SC_MapInfoPacket* packet);

    // 클라이언트에게 고유 ID(0~2) 부여
    bool SendAssignIDPacket(int client_id, u_short id);

    // 이벤트 패킷 전송 (스테이지 클리어 등)
    bool SendEventPacket(int client_id, E_EventType event_type);



private:

    SOCKET m_listen;                     // 서버 리슨 소켓
    SOCKET m_clients[MAX_PLAYERS];       // 접속 중인 각 클라이언트 소켓 배열

    CRITICAL_SECTION m_cs;               // 소켓 배열 보호용 임계영역 (멀티스레드-safe)




    // 클라이언트별 Recv 스레드 생성
    void StartRecvThread(int client_id);

    // 모든 클라에게 맵 정보 전송
    void BroadcastMapInfo();



    // 게임 데이터
    Player server_players[MAX_PLAYERS];  // 서버 authoritative 플레이어 상태
    Map    server_map[4];                   // 현재 게임 맵 상태(디폴트 맵에 사용되는 4개의 맵 구조체를
                                            // 게임 시작 전에 미리 생성, now_map 변수같은걸로 현재 진행 맵 확인)
                                            // 제작 맵과 기본 맵을 하나의 배열로 사용할거니까 처음 스타트세션 패킷 받을 때
                                            // 구분한 게임 타입(기본, 제작) 에 따라서 now_map 등 다르게 처리
    size_t now_map;
    int map_type;
    // 기본맵 로딩
    void LoadDefaultMap(int map_num);



    // 서버 주요 게임 루프
    void StartGameLoop();                // 게임 루프 시작
    bool game_loop_running = false;      // 게임 루프 실행 여부


    // 루프 내 처리 함수들
    void UpdateAllPositions();           // 서버 authoritative 이동
    void CheckAllCollisions();           // 충돌 판정
    bool BroadcastGameState();           // 모든 클라이언트에게 상태 전송

    void Make_Defalt_Map();              // 기본 맵의 모든 정보 초기화
};