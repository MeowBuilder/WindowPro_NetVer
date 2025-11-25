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
        SC_AssignIDPacket pkt(0);
        printf("\n--- TEST: SC_AssignIDPacket (before Encode) ---\n");
        pkt.Log(); // host byte order

        pkt.Encode();
        int sent = send(client, (char*)&pkt, sizeof(pkt), 0);
        printf("[SERVER] Sent SC_AssignIDPacket: %d bytes\n", sent);
    }

    SC_MapInfoPacket pkt;
    Map& new_map = pkt.mapInfo;
	int randomnum;
	new_map.block_count = new_map.object_count = new_map.enemy_count = new_map.boss_count = 0;
    //맵 초기화 및 생성
    
    RECT Desk_rect = { 0,0,1920,1080 };
	//맵 블럭 초기화
	new_map.blocks[new_map.block_count].x = Desk_rect.right / 2;
	new_map.blocks[new_map.block_count].y = (Desk_rect.bottom - 100);
	new_map.blocks[new_map.block_count].Block_rt = { Desk_rect.left,Desk_rect.bottom - 128,Desk_rect.right,Desk_rect.bottom };
	new_map.block_count++;

	new_map.blocks[new_map.block_count].x = Desk_rect.left + 448;
	new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 96;
	new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 128,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 128,new_map.blocks[new_map.block_count].y + 32 };
	new_map.block_count++;

	for (int i = 0; i < (Desk_rect.right - 896) / 384; i++)
	{
		randomnum = rand() % 2;
		if (randomnum == 0)
		{
			new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 384;
			new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 192;
			new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 128,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 128,new_map.blocks[new_map.block_count].y + 32 };
			new_map.block_count++;
		}
		else
		{
			new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 384;
			new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 96;
			new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 128,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 128,new_map.blocks[new_map.block_count].y + 32 };
			new_map.block_count++;
		}
	}

	//맵 오브젝트 초기화
	new_map.objects[new_map.object_count].x = Desk_rect.left + 320;
	new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
	new_map.objects[new_map.object_count].obj_type = Spike;
	new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
	new_map.object_count++;

	for (int i = 0; i < (Desk_rect.right - 640) / (Size * 2); i++)
	{
		new_map.objects[new_map.object_count].x = new_map.objects[new_map.object_count - 1].x + (Size * 2);
		new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
		new_map.objects[new_map.object_count].obj_type = Spike;
		new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
		new_map.object_count++;
	}

	new_map.objects[new_map.object_count].x = new_map.objects[new_map.object_count - 1].x + 48;
	new_map.objects[new_map.object_count].y = new_map.blocks[0].Block_rt.top - 8;
	new_map.objects[new_map.object_count].obj_type = Flag;
	new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
	new_map.object_count++;

	//플레이어 생성
	new_map.P_Start_Loc[0].x = Desk_rect.left + 64;
	new_map.P_Start_Loc[0].y = Desk_rect.bottom - 128;


    printf("\n--- TEST: SC_MapInfoPacket (before Encode) ---\n");
    pkt.Log();

    pkt.Encode();
    int sent = send(client, (char*)&pkt, sizeof(pkt), 0);
    printf("[SERVER] Sent SC_MapInfoPacket: %d bytes (sizeof=%zu)\n", sent, sizeof(pkt));

    printf("\n[SERVER] Test sequence done. Closing.\n");

    closesocket(client);
    closesocket(listenSock);
    WSACleanup();
    return 0;
}