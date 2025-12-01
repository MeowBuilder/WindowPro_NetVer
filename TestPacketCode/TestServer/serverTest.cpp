#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstdlib>

#include "Packets.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 9000

int ReceiveData(SOCKET sock, char* buf, int len)
{
    int received = 0;
    while (received < len)
    {
        int ret = recv(sock, buf + received, len - received, 0);
        if (ret <= 0)
        {
            return ret; // 0:  , <0: 
        }
        received += ret;
    }
    return received;
}

// 패킷 타입에 따라 처리하는 함수
void ProcessPacket(SOCKET client, char* packet_buf, bool& bLoop)
{
    BasePacket* base_p = (BasePacket*)packet_buf;
    printf("\n--- RECV (Server): Packet Type %d ---\n", base_p->type);

    switch (base_p->type)
    {
    case CS_START_SESSION_REQ:
    {
        CS_StartSessionRequestPacket* p = (CS_StartSessionRequestPacket*)packet_buf;
        p->Decode();
        p->Log();

        printf("\n--- SEND (Server): SC_MapInfoPacket ---\n");
        SC_MapInfoPacket pkt;
        // 테스트용 맵 정보 채우기 (올바른 방식)
        pkt.mapInfo.block_count = 1;
        pkt.mapInfo.blocks[0].x = 100;
        pkt.mapInfo.blocks[0].y = 200;
        pkt.mapInfo.blocks[0].Block_rt = { 0, 0, 32, 32 };
        pkt.Log(); // Log before Encode
        pkt.Encode();
        send(client, (char*)&pkt, sizeof(pkt), 0);
        break;
    }
    case CS_UPLOAD_MAP:
    {
        CS_UploadMapPacket* p = (CS_UploadMapPacket*)packet_buf;
        p->Decode();
        p->Log();

        printf("\n--- SEND (Server): SC_MapUploadResponsePacket ---\n");
        SC_MapUploadResponsePacket pkt(true);
        pkt.Log(); // Log before Encode
        pkt.Encode();
        send(client, (char*)&pkt, sizeof(pkt), 0);
        break;
    }
    case CS_PLAYER_UPDATE:
    {
        CS_PlayerUpdatePacket* p = (CS_PlayerUpdatePacket*)packet_buf;
        p->Decode();
        p->Log();

        printf("\n--- SEND (Server): SC_GameStatePacket ---\n");
        SC_GameStatePacket state_pkt;
        // 테스트용 게임 상태 정보 채우기
        state_pkt.players[p->player_id].is_connected = true;
        state_pkt.players[p->player_id].pos = p->pos;
        state_pkt.Encode();
        send(client, (char*)&state_pkt, sizeof(state_pkt), 0);

        printf("\n--- SEND (Server): SC_EventPacket ---\n");
        SC_EventPacket event_pkt(STAGE_CLEAR);
        event_pkt.Encode();
        send(client, (char*)&event_pkt, sizeof(event_pkt), 0);
        break;
    }
    case CS_END_SESSION_REQ:
    {
        CS_EndSessionRequestPacket* p = (CS_EndSessionRequestPacket*)packet_buf;
        p->Decode();
        p->Log();

        printf("\n--- SEND (Server): SC_DisconnectPacket ---\n");
        SC_DisconnectPacket pkt(p->player_id);
        pkt.Log(); // Log before Encode
        pkt.Encode();
        send(client, (char*)&pkt, sizeof(pkt), 0);
        bLoop = false; // 루프 종료 플래그
        break;
    }
    default:
        printf("[SERVER] Received unhandled packet type: %d\n", base_p->type);
        break;
    }
}


int main()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET)
    {
        printf("socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // SO_REUSEADDR 옵션 설정
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        printf("bind() failed: %d\n", WSAGetLastError());
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        printf("listen() failed: %d\n", WSAGetLastError());
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    printf("[SERVER] READY. Waiting for a client...\n");

    sockaddr_in clientAddr = {};
    int clientLen = sizeof(clientAddr);
    SOCKET client = accept(listenSock, (sockaddr*)&clientAddr, &clientLen);
    if (client == INVALID_SOCKET)
    {
        printf("accept() failed: %d\n", WSAGetLastError());
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }
    printf("[SERVER] Client connected.\n");

    // 1. 초기 패킷 전송 (ID 할당, 플레이어 접속)
    {
        printf("\n--- SEND (Server): SC_AssignIDPacket ---\n");
        SC_AssignIDPacket pkt(0); // 테스트 클라이언트는 ID 0번
        pkt.Log(); // Log before Encode
        pkt.Encode();
        send(client, (char*)&pkt, sizeof(pkt), 0);
    }
    {
        printf("\n--- SEND (Server): SC_PlayerJoinPacket ---\n");
        SC_PlayerJoinPacket pkt(0); // 0번 플레이어 접속 알림
        pkt.Log(); // Log before Encode
        pkt.Encode();
        send(client, (char*)&pkt, sizeof(pkt), 0);
    }

    // 2. 클라이언트로부터 패킷 수신 및 응답 루프
    char streamBuf[4096] = { 0, };
    int streamLen = 0;
    bool bLoop = true;

    while (bLoop)
    {
        char recvBuf[1024];
        int recv_len = recv(client, recvBuf, sizeof(recvBuf), 0);
        if (recv_len <= 0)
        {
            printf("[SERVER] Client disconnected or recv error: %d\n", WSAGetLastError());
            break;
        }

        memcpy(streamBuf + streamLen, recvBuf, recv_len);
        streamLen += recv_len;

        int offset = 0;
        while (streamLen - offset >= sizeof(BasePacket))
        {
            BasePacket* header = (BasePacket*)(streamBuf + offset);
            uint16_t packetSize = ntohs(header->size);

            if (packetSize > 0 && streamLen - offset >= packetSize)
            {
                char packet_buf[16384];
                memcpy(packet_buf, streamBuf + offset, packetSize);

                ProcessPacket(client, packet_buf, bLoop);
                
                offset += packetSize;

                if (!bLoop) break;
            }
            else
            {
                break;
            }
        }

        if (offset > 0)
        {
            int remain = streamLen - offset;
            if (remain > 0)
            {
                memmove(streamBuf, streamBuf + offset, remain);
            }
            streamLen = remain;
        }
    }


    printf("\n[SERVER] Test sequence done. Closing.\n");

    closesocket(client);
    closesocket(listenSock);
    WSACleanup();
    return 0;
}