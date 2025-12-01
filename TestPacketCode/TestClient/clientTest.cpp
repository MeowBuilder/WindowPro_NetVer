#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstdlib>

#include "Packets.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 9000

int ReceiveData(SOCKET sock, char* buf, int len)
{
    int rec = 0;
    while (rec < len)
    {
        int ret = recv(sock, buf + rec, len - rec, 0);
        if (ret <= 0) return ret;
        rec += ret;
    }
    return rec;
}

// 서버로부터 정확한 크기의 패킷 하나를 수신하는 헬퍼 함수
// 반환값: 패킷 타입, 오류 시 -1
char RecvOnePacket(SOCKET sock, char* outBuf, int bufSize)
{
    // 1. 헤더 먼저 수신
    char headerBuf[sizeof(BasePacket)];
    if (ReceiveData(sock, headerBuf, sizeof(BasePacket)) != sizeof(BasePacket)) {
        printf("[Error] RecvOnePacket: Failed to receive header.\n");
        return -1;
    }

    BasePacket* header = (BasePacket*)headerBuf;
    // 중요: 서버가 Encode()로 보낸 size는 네트워크 바이트 순서이므로, ntohs()로 변환해야 함
    uint16_t packetSize = ntohs(header->size);

    if (packetSize == 0 || packetSize > bufSize)
    {
        printf("[Error] RecvOnePacket: Invalid packet size: %hu\n", packetSize);
        return -1;
    }

    // 2. 헤더 내용을 최종 버퍼에 복사
    memcpy(outBuf, headerBuf, sizeof(BasePacket));

    // 3. 나머지 데이터 수신
    int remain_len = packetSize - sizeof(BasePacket);
    if (remain_len > 0)
    {
        if (ReceiveData(sock, outBuf + sizeof(BasePacket), remain_len) != remain_len) {
            printf("[Error] RecvOnePacket: Failed to receive remaining body.\n");
            return -1;
        }
    }

    // 4. 디코딩 및 타입 반환
    BasePacket* p = (BasePacket*)outBuf;
    
    // 각 패킷 타입에 맞춰 Decode() 호출
    // 이 작업은 패킷을 사용하기 전에 반드시 필요함!
    switch(p->type)
    {
        case SC_ASSIGN_ID:       ((SC_AssignIDPacket*)p)->Decode(); break;
        case SC_PLAYER_JOIN:     ((SC_PlayerJoinPacket*)p)->Decode(); break;
        case SC_MAP_INFO:        ((SC_MapInfoPacket*)p)->Decode(); break;
        case SC_MAP_UPLOAD_RSP:  ((SC_MapUploadResponsePacket*)p)->Decode(); break;
        case SC_GAME_STATE:      ((SC_GameStatePacket*)p)->Decode(); break;
        case SC_EVENT:           ((SC_EventPacket*)p)->Decode(); break;
        case SC_DISCONNECT:      ((SC_DisconnectPacket*)p)->Decode(); break;
        default: printf("[Warning] RecvOnePacket: Unknown packet type %d for decoding.\n", p->type);
    }
    
    return p->type;
}


int main()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        printf("socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        printf("connect() failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("[CLIENT] Connected to server.\n");

    u_short my_id = (u_short)-1;
    char packetBuf[16384]; // 충분한 크기의 버퍼

    // 1. 초기 패킷 수신 (ID 할당, 플레이어 접속)
    printf("\n=== 1. Receiving initial packets ===\n");
    if (RecvOnePacket(sock, packetBuf, sizeof(packetBuf)) == SC_ASSIGN_ID) {
        SC_AssignIDPacket* p = (SC_AssignIDPacket*)packetBuf;
        my_id = p->player_id;
        printf("--- RECV: SC_AssignIDPacket ---\n");
        p->Log();
    }
    if (RecvOnePacket(sock, packetBuf, sizeof(packetBuf)) == SC_PLAYER_JOIN) {
        SC_PlayerJoinPacket* p = (SC_PlayerJoinPacket*)packetBuf;
        printf("--- RECV: SC_PlayerJoinPacket ---\n");
        p->Log();
    }

    // 2. 세션 시작 요청 및 맵 정보 수신
    printf("\n=== 2. Testing Session Start ===\n");
    {
        CS_StartSessionRequestPacket pkt(0); // map_type 0
        pkt.Encode();
        send(sock, (char*)&pkt, sizeof(pkt), 0);
        printf("--- SENT: CS_StartSessionRequestPacket ---\n");
        pkt.Log();
    }
    if (RecvOnePacket(sock, packetBuf, sizeof(packetBuf)) == SC_MAP_INFO) {
        SC_MapInfoPacket* p = (SC_MapInfoPacket*)packetBuf;
        printf("--- RECV: SC_MapInfoPacket ---\n");
        p->Log();
    }

    // 3. 맵 업로드 및 응답 수신
    printf("\n=== 3. Testing Map Upload ===\n");
    {
        CS_UploadMapPacket pkt;
        pkt.UploadMap.block_count = 1;
        pkt.UploadMap.blocks[0].x = 99;
        pkt.Encode();
        send(sock, (char*)&pkt, sizeof(pkt), 0);
        printf("--- SENT: CS_UploadMapPacket ---\n");
    }
    if (RecvOnePacket(sock, packetBuf, sizeof(packetBuf)) == SC_MAP_UPLOAD_RSP) {
        SC_MapUploadResponsePacket* p = (SC_MapUploadResponsePacket*)packetBuf;
        printf("--- RECV: SC_MapUploadResponsePacket ---\n");
        p->Log();
    }

    // 4. 플레이어 상태 업데이트 및 게임 상태/이벤트 수신
    printf("\n=== 4. Testing Player Update ===\n");
    {
        CS_PlayerUpdatePacket pkt;
        pkt.player_id = my_id;
        pkt.pos = { 123, 456 };
        pkt.Encode();
        send(sock, (char*)&pkt, sizeof(pkt), 0);
        printf("--- SENT: CS_PlayerUpdatePacket ---\n");
    }
    if (RecvOnePacket(sock, packetBuf, sizeof(packetBuf)) == SC_GAME_STATE) {
        SC_GameStatePacket* p = (SC_GameStatePacket*)packetBuf;
        printf("--- RECV: SC_GameStatePacket ---\n");
        // p->Log(); // 너무 길어서 생략
    }
    if (RecvOnePacket(sock, packetBuf, sizeof(packetBuf)) == SC_EVENT) {
        SC_EventPacket* p = (SC_EventPacket*)packetBuf;
        printf("--- RECV: SC_EventPacket ---\n");
        p->Log();
    }

    // 5. 세션 종료 요청 및 접속 종료 수신
    printf("\n=== 5. Testing Session End ===\n");
    {
        CS_EndSessionRequestPacket pkt(my_id);
        pkt.Encode();
        send(sock, (char*)&pkt, sizeof(pkt), 0);
        printf("--- SENT: CS_EndSessionRequestPacket ---\n");
    }
    if (RecvOnePacket(sock, packetBuf, sizeof(packetBuf)) == SC_DISCONNECT) {
        SC_DisconnectPacket* p = (SC_DisconnectPacket*)packetBuf;
        printf("--- RECV: SC_DisconnectPacket ---\n");
        p->Log();
    }


    printf("\n[CLIENT] Test sequence finished. Closing.\n");

    closesocket(sock);
    WSACleanup();
    return 0;
}