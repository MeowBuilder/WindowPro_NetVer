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

int RecvAll(SOCKET sock, char* buf, int len)
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

    printf("[CLIENT] Connected to server.\n\n");

    u_short my_id = (u_short)-1; // 클라이언트 ID 저장
    char buf[20000];

    auto RecvPacket = [&](int expectedSize)
        {
            int ret = RecvAll(sock, buf, expectedSize);
            if (ret != expectedSize)
            {
                printf("[CLIENT] Recv failed: %d / %d\n", ret, expectedSize);
                return;
            }

            BasePacket* b = (BasePacket*)buf;

            switch (b->type)
            {
            case SC_ASSIGN_ID:
            {
                SC_AssignIDPacket* p = (SC_AssignIDPacket*)buf;
                p->Decode();
                my_id = p->player_id; // ID 저장
                printf("--- RECV: SC_AssignIDPacket ---\n");
                p->Log();
                break;
            }
            case SC_MAP_UPLOAD_RSP:
            {
                SC_MapUploadResponsePacket* p = (SC_MapUploadResponsePacket*)buf;
                p->Decode();
                printf("--- RECV: SC_MapUploadResponsePacket ---\n");
                p->Log();
                break;
            }
            case SC_MAP_INFO:
            {
                SC_MapInfoPacket* p = (SC_MapInfoPacket*)buf;
                p->Decode();
                printf("--- RECV: SC_MapInfoPacket ---\n");
                p->Log();
                break;
            }
            case SC_GAME_STATE:
            {
                SC_GameStatePacket* p = (SC_GameStatePacket*)buf;
                p->Decode();
                printf("--- RECV: SC_GameStatePacket ---\n");
                p->Log();
                break;
            }
            case SC_EVENT:
            {
                SC_EventPacket* p = (SC_EventPacket*)buf;
                p->Decode();
                printf("--- RECV: SC_EventPacket ---\n");
                p->Log();
                break;
            }
            default:
                printf("[CLIENT] Unknown packet type: %d\n", b->type);
                break;
            }
        };

    // -------------------------------
    // S -> C :  SC_* Ŷ 
    // -------------------------------
    printf("=== Receiving all SC packets ===\n");
    RecvPacket(sizeof(SC_AssignIDPacket));
    RecvPacket(sizeof(SC_MapUploadResponsePacket));
    RecvPacket(sizeof(SC_MapInfoPacket));
    RecvPacket(sizeof(SC_GameStatePacket));
    RecvPacket(sizeof(SC_EventPacket));
    RecvPacket(sizeof(SC_EventPacket));

    // -------------------------------
    // C -> S :  CS_* Ŷ 
    // -------------------------------
    printf("\n=== Sending ALL CS packets ===\n");

    // 1) CS_StartSessionRequestPacket
    {
        CS_StartSessionRequestPacket pkt;
        printf("\n--- SEND: CS_StartSessionRequestPacket (before Encode) ---\n");
        pkt.Log();

        pkt.Encode();
        int sent = send(sock, (char*)&pkt, sizeof(pkt), 0);
        printf("[CLIENT] Sent CS_StartSessionRequestPacket: %d bytes\n", sent);
    }

    // 2) CS_UploadMapPacket ()
    {
        CS_UploadMapPacket pkt;
        
        Map& testMap = pkt.UploadMap; // Get a reference for easier access
        testMap.block_count = 2;
        testMap.blocks[0].x = 111;
        testMap.blocks[0].y = 222;
        testMap.blocks[0].Block_rt = { 0, 0, 32, 32 };

        testMap.blocks[1].x = 333;
        testMap.blocks[1].y = 444;
        testMap.blocks[1].Block_rt = { 32, 32, 64, 64 };

        testMap.object_count = 1;
        testMap.objects[0].x = 777;
        testMap.objects[0].y = 888;
        testMap.objects[0].obj_type = Flag;
        testMap.objects[0].Obj_rt = { 0, 0, 16, 16 };

        testMap.enemy_count = 1;
        testMap.enemys[0].x = 999;
        testMap.enemys[0].y = 1000;
        // The other fields of enemy are not initialized, but that's fine for this test.

        testMap.P_Start_Loc[0].x = 10;
        testMap.P_Start_Loc[0].y = 20;

        printf("\n--- SEND: CS_UploadMapPacket (before Encode) ---\n");
        pkt.Log();

        pkt.Encode();
        int sent = send(sock, (char*)&pkt, sizeof(pkt), 0);
        printf("[CLIENT] Sent CS_UploadMapPacket: %d bytes (sizeof=%zu)\n", sent, sizeof(pkt));
    }

    // 3) CS_EndSessionRequestPacket
    {
        if (my_id != (u_short)-1)
        {
            CS_EndSessionRequestPacket pkt(my_id);
            printf("\n--- SEND: CS_EndSessionRequestPacket (before Encode) ---\n");
            pkt.Log();

            pkt.Encode();
            int sent = send(sock, (char*)&pkt, sizeof(pkt), 0);
            printf("[CLIENT] Sent CS_EndSessionRequestPacket: %d bytes\n", sent);
        }
        else
        {
            printf("\n[CLIENT] Cannot send CS_EndSessionRequestPacket: No ID assigned.\n");
        }
    }


    printf("\n[CLIENT] All CS packets sent. Closing.\n");

    closesocket(sock);
    WSACleanup();
    return 0;
}