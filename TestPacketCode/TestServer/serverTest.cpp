#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstdlib>

#include "Packets.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 9000

int RecvAll(SOCKET sock, char* buf, int len)
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

    printf("[SERVER] READY. Waiting...\n");

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

    // -------------------------------
    // S -> C :  SC_* Ŷ 
    // -------------------------------

    // 1) SC_AssignIDPacket
    {
        SC_AssignIDPacket pkt(1);
        printf("\n--- TEST: SC_AssignIDPacket (before Encode) ---\n");
        pkt.Log(); // host byte order

        pkt.Encode();
        int sent = send(client, (char*)&pkt, sizeof(pkt), 0);
        printf("[SERVER] Sent SC_AssignIDPacket: %d bytes\n", sent);
    }

    // 2) SC_MapUploadResponsePacket
    {
        SC_MapUploadResponsePacket pkt(true);
        printf("\n--- TEST: SC_MapUploadResponsePacket (before Encode) ---\n");
        pkt.Log();

        pkt.Encode();
        int sent = send(client, (char*)&pkt, sizeof(pkt), 0);
        printf("[SERVER] Sent SC_MapUploadResponsePacket: %d bytes\n", sent);
    }

    // 3) SC_MapInfoPacket (  ä)
    {
        SC_MapInfoPacket pkt;
        pkt.block_count = 2;
        pkt.blocks[0].x = 100;
        pkt.blocks[0].y = 200;
        pkt.blocks[0].Block_rt = { 0, 0, 32, 32 };

        pkt.blocks[1].x = 300;
        pkt.blocks[1].y = 400;
        pkt.blocks[1].Block_rt = { 32, 32, 64, 64 };

        pkt.object_count = 1;
        pkt.objects[0].x = 500;
        pkt.objects[0].y = 600;
        pkt.objects[0].obj_type = Spike;
        pkt.objects[0].Obj_rt = { 0, 0, 16, 16 };

        pkt.enemy_spawn_count = 1;
        pkt.enemy_spawns[0].x = 700;
        pkt.enemy_spawns[0].y = 800;

        pkt.player_start_pos[0].x = 10;
        pkt.player_start_pos[0].y = 20;

        printf("\n--- TEST: SC_MapInfoPacket (before Encode) ---\n");
        pkt.Log();

        pkt.Encode();
        int sent = send(client, (char*)&pkt, sizeof(pkt), 0);
        printf("[SERVER] Sent SC_MapInfoPacket: %d bytes (sizeof=%zu)\n", sent, sizeof(pkt));
    }

    // 4) SC_GameStatePacket ( )
    {
        SC_GameStatePacket pkt;
        pkt.players[0].is_connected = true;
        pkt.players[0].pos.x = 300;
        pkt.players[0].pos.y = 150;
        pkt.players[0].life = 99;
        pkt.players[0].walk_state = 1;
        pkt.players[0].jump_state = 0;
        pkt.players[0].frame_counter = 10;
        pkt.players[0].dir = Direction::RIGHT;

        pkt.enemies[0].is_alive = true;
        pkt.enemies[0].pos.x = 1000;
        pkt.enemies[0].pos.y = 2000;
        pkt.enemies[0].dir = Direction::LEFT;
        pkt.enemies[0].move_state = 2;

        pkt.boss.is_active = true;
        pkt.boss.pos.x = 5000;
        pkt.boss.pos.y = 6000;
        pkt.boss.life = 123;
        pkt.boss.attack_time = 42;
        pkt.boss.dir = Direction::RIGHT;

        printf("\n--- TEST: SC_GameStatePacket (before Encode) ---\n");
        pkt.Log();

        pkt.Encode();
        int sent = send(client, (char*)&pkt, sizeof(pkt), 0);
        printf("[SERVER] Sent SC_GameStatePacket: %d bytes (sizeof=%zu)\n", sent, sizeof(pkt));
    }

    // 5) SC_EventPacket (STAGE_CLEAR)
    {
        SC_EventPacket pkt(STAGE_CLEAR);
        printf("\n--- TEST: SC_EventPacket (STAGE_CLEAR) (before Encode) ---\n");
        pkt.Log();

        pkt.Encode();
        int sent = send(client, (char*)&pkt, sizeof(pkt), 0);
        printf("[SERVER] Sent SC_EventPacket: %d bytes\n", sent);
    }

    // 6) SC_EventPacket (GAME_WIN)
    {
        SC_EventPacket pkt(GAME_WIN);
        printf("\n--- TEST: SC_EventPacket (GAME_WIN) (before Encode) ---\n");
        pkt.Log();

        pkt.Encode();
        int sent = send(client, (char*)&pkt, sizeof(pkt), 0);
        printf("[SERVER] Sent SC_EventPacket: %d bytes\n", sent);
    }

    // -------------------------------
    // C -> S :  CS_* Ŷ 
    // -------------------------------

    char buf[20000]; //  ū Ŷ ˳

    // 1) CS_StartSessionRequestPacket 
    {
        int expected = sizeof(CS_StartSessionRequestPacket);
        int ret = RecvAll(client, buf, expected);
        if (ret != expected)
        {
            printf("[SERVER] Recv CS_StartSessionRequestPacket failed: %d / %d\n", ret, expected);
        }
        else
        {
            CS_StartSessionRequestPacket* p = (CS_StartSessionRequestPacket*)buf;
            p->Decode();
            printf("\n--- RECV: CS_StartSessionRequestPacket ---\n");
            p->Log();
        }
    }

    // 2) CS_UploadMapPacket 
    {
        int expected = sizeof(CS_UploadMapPacket);
        int ret = RecvAll(client, buf, expected);
        if (ret != expected)
        {
            printf("[SERVER] Recv CS_UploadMapPacket failed: %d / %d\n", ret, expected);
        }
        else
        {
            CS_UploadMapPacket* p = (CS_UploadMapPacket*)buf;
            p->Decode();
            printf("\n--- RECV: CS_UploadMapPacket ---\n");
            p->Log();
        }
    }

    printf("\n[SERVER] Test sequence done. Closing.\n");

    closesocket(client);
    closesocket(listenSock);
    WSACleanup();
    return 0;
}