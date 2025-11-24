#ifndef PACKETS_H
#define PACKETS_H

#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

#include "GameManager.h"
#pragma comment(lib, "ws2_32.lib")

// --- 패킷 타입 정의 ---
// Client -> Server
constexpr char CS_UPLOAD_MAP = 0;
constexpr char CS_START_SESSION_REQ = 1;
constexpr char CS_PLAYER_UPDATE = 2;
constexpr char CS_END_SESSION_REQ = 3;

// Server -> Client
constexpr char SC_ASSIGN_ID = 10;
constexpr char SC_MAP_UPLOAD_RSP = 11;
constexpr char SC_MAP_INFO = 12;
constexpr char SC_GAME_STATE = 13;
constexpr char SC_EVENT = 14;
constexpr char SC_DISCONNECT = 15;

// --- 이벤트 타입 정의 (프로젝트 계획서 기반) ---
enum E_EventType { STAGE_CLEAR, GAME_WIN };

/*
 * --- 패킷 클래스 사용 가이드  ---
 *
 * 1. 송신 (데이터 보내기)
 *    - 보낼 패킷 객체를 생성하고 내용을 채웁니다. (예: SC_AssignIDPacket packet(my_id);)
 *    - packet.Encode(); 메소드를 호출하여 패킷 내부의 모든 필드를 네트워크 바이트 순서로 변환합니다.
 *    - send() 함수를 사용하여 패킷을 전송합니다.
 *      - **중요**: 길이는 반드시 sizeof(packet)을 사용해야 합니다.
 *      - packet.size 필드는 Encode() 후 네트워크 바이트 순서로 변경되어 있으므로 길이로 사용하면 안 됩니다.
 *      - send(socket, (char*)&packet, sizeof(packet), 0);
 *
 * 2. 수신 (데이터 받기)
 *    - recv() 함수를 사용하여 char 배열(버퍼)에 데이터를 받습니다.
 *    - 버퍼의 주소를 BasePacket 포인터로 형변환하여 type 필드만 확인합니다.
 *      - BasePacket* base_p = (BasePacket*)buffer;
 *    - type에 따라 switch 문으로 분기합니다.
 *    - 각 case 내부에서 해당 타입으로 캐스팅 후 Decode()를 호출합니다.
 *      - case SC_GAME_STATE:
 *        {
 *            SC_GameStatePacket* p = (SC_GameStatePacket*)buffer;
 *            p->Decode(); // 수신된 데이터의 바이트 순서를 호스트 순서로 변환
 *            // 이제 p->players 등 멤버를 안전하게 사용합니다.
 *        }
 *        break;

 *
 * 3. 디버깅
 *    - 특정 패킷 객체(또는 포인터)의 Log() 메소드를 호출하여 현재 내용을 콘솔에 출력할 수 있습니다.
 *      - 예: p->Log();
 */

// --- 패킷 구조체 정의 ---
#pragma pack(push, 1) // 네트워크 패킷 직렬화를 위해 구조체 패딩을 1바이트로 설정

// 모든 패킷의 기반이 되는 클래스 (Non-virtual)
class BasePacket {
public:
    uint16_t size; // 패킷의 전체 크기 (size_t -> uint16_t로 변경)
    char type;   // 패킷의 종류

    // 생성자
    BasePacket() : size(0), type(0) {} // 실제 크기와 타입은 파생 클래스에서 설정
};

// [S->C] 서버가 클라이언트에게 고유 ID를 할당하는 패킷
class SC_AssignIDPacket : public BasePacket {
public:
    u_short player_id; // 플레이어에게 할당될 고유 ID

    SC_AssignIDPacket() {
        size = sizeof(SC_AssignIDPacket);
        type = SC_ASSIGN_ID;
        player_id = (u_short)-1;
    }

    SC_AssignIDPacket(u_short id) {
        size = sizeof(SC_AssignIDPacket);
        type = SC_ASSIGN_ID;
        player_id = id;
    }

    void Encode() {
        size = htons(size);
        player_id = htons(player_id);
    }

    void Decode() {
        size = ntohs(size);
        player_id = ntohs(player_id);
    }

    void Log() const {
        printf("[SC_AssignIDPacket] Type: %d, Size: %hu, PlayerID: %hu\n", type, size, player_id);
    }
};

// [S->C] 맵 업로드 결과를 클라이언트에게 응답하는 패킷
class SC_MapUploadResponsePacket : public BasePacket {
public:
    bool is_success;

    SC_MapUploadResponsePacket() {
        size = sizeof(SC_MapUploadResponsePacket);
        type = SC_MAP_UPLOAD_RSP;
        is_success = false;
    }

    SC_MapUploadResponsePacket(bool success) {
        size = sizeof(SC_MapUploadResponsePacket);
        type = SC_MAP_UPLOAD_RSP;
        is_success = success;
    }

    void Encode() {
        size = htons(size);
    }
    void Decode() {
        size = ntohs(size);
    }

    void Log() const {
        printf("[SC_MapUploadResponsePacket] Type: %d, Size: %hu, Success: %s\n", type, size, is_success ? "true" : "false");
    }
};

// [C->S] 클라이언트가 편집한 맵 정보를 서버로 업로드하는 패킷
class CS_UploadMapPacket : public BasePacket {
public:
    Map UploadMap;

    CS_UploadMapPacket() {
        size = sizeof(CS_UploadMapPacket);
        type = CS_UPLOAD_MAP;
        memset(&UploadMap, 0, sizeof(Map));
    }

    void Init(const Map& map) {
        UploadMap = map;
        // Ensure size and type are set correctly after initialization
        size = sizeof(CS_UploadMapPacket);
        type = CS_UPLOAD_MAP;
    }

    void ConvertRectEndian(RECT& rect) {
        rect.left = htonl(rect.left);
        rect.top = htonl(rect.top);
        rect.right = htonl(rect.right);
        rect.bottom = htonl(rect.bottom);
    }

    void RestoreRectEndian(RECT& rect) {
        rect.left = ntohl(rect.left);
        rect.top = ntohl(rect.top);
        rect.right = ntohl(rect.right);
        rect.bottom = ntohl(rect.bottom);
    }

    void Encode() {
        size = htons(size);

        UploadMap.Map_width = htonl(UploadMap.Map_width);
        UploadMap.Map_height = htonl(UploadMap.Map_height);
        UploadMap.block_count = htonl(UploadMap.block_count);
        UploadMap.object_count = htonl(UploadMap.object_count);
        UploadMap.enemy_count = htonl(UploadMap.enemy_count);
        UploadMap.boss_count = htonl(UploadMap.boss_count);

        for (int i = 0; i < 160; ++i) {
            UploadMap.blocks[i].x = htonl(UploadMap.blocks[i].x);
            UploadMap.blocks[i].y = htonl(UploadMap.blocks[i].y);
            ConvertRectEndian(UploadMap.blocks[i].Block_rt);
        }

        for (int i = 0; i < 160; ++i) {
            UploadMap.objects[i].x = htonl(UploadMap.objects[i].x);
            UploadMap.objects[i].y = htonl(UploadMap.objects[i].y);
            UploadMap.objects[i].obj_type = (Object_type)htonl(UploadMap.objects[i].obj_type);
            ConvertRectEndian(UploadMap.objects[i].Obj_rt);
        }

        for (int i = 0; i < 32; ++i) {
            UploadMap.enemys[i].x = htonl(UploadMap.enemys[i].x);
            UploadMap.enemys[i].y = htonl(UploadMap.enemys[i].y);
            ConvertRectEndian(UploadMap.enemys[i].enemy_rect);
            UploadMap.enemys[i].direction = (Direction)htonl(UploadMap.enemys[i].direction);
            UploadMap.enemys[i].move_state = htonl(UploadMap.enemys[i].move_state);
            UploadMap.enemys[i].on_block = htonl(UploadMap.enemys[i].on_block);
        }
        
        UploadMap.boss.x = htonl(UploadMap.boss.x);
        UploadMap.boss.y = htonl(UploadMap.boss.y);
        ConvertRectEndian(UploadMap.boss.boss_rect);
        UploadMap.boss.life = htonl(UploadMap.boss.life);
        UploadMap.boss.attack_time = htonl(UploadMap.boss.attack_time);

        for (int i = 0; i < 3; ++i) {
            UploadMap.P_Start_Loc[i].x = htonl(UploadMap.P_Start_Loc[i].x);
            UploadMap.P_Start_Loc[i].y = htonl(UploadMap.P_Start_Loc[i].y);
        }
    }

    void Decode() {
        size = ntohs(size);

        UploadMap.Map_width = ntohl(UploadMap.Map_width);
        UploadMap.Map_height = ntohl(UploadMap.Map_height);
        UploadMap.block_count = ntohl(UploadMap.block_count);
        UploadMap.object_count = ntohl(UploadMap.object_count);
        UploadMap.enemy_count = ntohl(UploadMap.enemy_count);
        UploadMap.boss_count = ntohl(UploadMap.boss_count);

        for (int i = 0; i < 160; ++i) {
            UploadMap.blocks[i].x = ntohl(UploadMap.blocks[i].x);
            UploadMap.blocks[i].y = ntohl(UploadMap.blocks[i].y);
            RestoreRectEndian(UploadMap.blocks[i].Block_rt);
        }

        for (int i = 0; i < 160; ++i) {
            UploadMap.objects[i].x = ntohl(UploadMap.objects[i].x);
            UploadMap.objects[i].y = ntohl(UploadMap.objects[i].y);
            UploadMap.objects[i].obj_type = (Object_type)ntohl(UploadMap.objects[i].obj_type);
            RestoreRectEndian(UploadMap.objects[i].Obj_rt);
        }

        for (int i = 0; i < 32; ++i) {
            UploadMap.enemys[i].x = ntohl(UploadMap.enemys[i].x);
            UploadMap.enemys[i].y = ntohl(UploadMap.enemys[i].y);
            RestoreRectEndian(UploadMap.enemys[i].enemy_rect);
            UploadMap.enemys[i].direction = (Direction)ntohl(UploadMap.enemys[i].direction);
            UploadMap.enemys[i].move_state = ntohl(UploadMap.enemys[i].move_state);
            UploadMap.enemys[i].on_block = ntohl(UploadMap.enemys[i].on_block);
        }

        UploadMap.boss.x = ntohl(UploadMap.boss.x);
        UploadMap.boss.y = ntohl(UploadMap.boss.y);
        RestoreRectEndian(UploadMap.boss.boss_rect);
        UploadMap.boss.life = ntohl(UploadMap.boss.life);
        UploadMap.boss.attack_time = ntohl(UploadMap.boss.attack_time);

        for (int i = 0; i < 3; ++i) {
            UploadMap.P_Start_Loc[i].x = ntohl(UploadMap.P_Start_Loc[i].x);
            UploadMap.P_Start_Loc[i].y = ntohl(UploadMap.P_Start_Loc[i].y);
        }
    }

    void Log() const {
        printf("[CS_UploadMapPacket] Type: %d, Size: %hu\n", type, size);
        printf("  Map Width: %d, Map Height: %d\n", UploadMap.Map_width, UploadMap.Map_height);
        printf("  Block Count: %d\n", UploadMap.block_count);
        for (int i = 0; i < UploadMap.block_count; ++i) {
            printf("    Block %d: Pos=(%d, %d), Rect=(%ld, %ld, %ld, %ld)\n",
                   i, UploadMap.blocks[i].x, UploadMap.blocks[i].y, UploadMap.blocks[i].Block_rt.left, UploadMap.blocks[i].Block_rt.top, UploadMap.blocks[i].Block_rt.right, UploadMap.blocks[i].Block_rt.bottom);
        }

        printf("  Object Count: %d\n", UploadMap.object_count);
        for (int i = 0; i < UploadMap.object_count; ++i) {
            printf("    Object %d: Pos=(%d, %d), Type=%d, Rect=(%ld, %ld, %ld, %ld)\n",
                   i, UploadMap.objects[i].x, UploadMap.objects[i].y, UploadMap.objects[i].obj_type, UploadMap.objects[i].Obj_rt.left, UploadMap.objects[i].Obj_rt.top, UploadMap.objects[i].Obj_rt.right, UploadMap.objects[i].Obj_rt.bottom);
        }

        printf("  Enemy Count: %d\n", UploadMap.enemy_count);
        for (int i = 0; i < UploadMap.enemy_count; ++i) {
            printf("    Enemy %d: Pos=(%d, %d)\n", i, UploadMap.enemys[i].x, UploadMap.enemys[i].y);
        }
        
        printf("  Boss Count: %d\n", UploadMap.boss_count);
        if (UploadMap.boss_count > 0) {
            printf("    Boss: Pos=(%d, %d), Life=%d\n", UploadMap.boss.x, UploadMap.boss.y, UploadMap.boss.life);
        }

        printf("  Player Start Positions:\n");
        for (int i = 0; i < 3; ++i) {
            printf("    Player %d: (%d, %d)\n", i, UploadMap.P_Start_Loc[i].x, UploadMap.P_Start_Loc[i].y);
        }
    }
};

// [C->S] 게임 세션 시작을 서버에 요청하는 패킷
class CS_StartSessionRequestPacket : public BasePacket {
public:
    CS_StartSessionRequestPacket() {
        size = sizeof(CS_StartSessionRequestPacket);
        type = CS_START_SESSION_REQ;
    }
    void Encode() {
        size = htons(size);
    }
    void Decode() {
        size = ntohs(size);
    }
    void Log() const {
        printf("[CS_StartSessionRequestPacket] Type: %d, Size: %hu\n", type, size);
    }
};

// [C->S] 게임 세션 종료를 서버에 요청하는 패킷
class CS_EndSessionRequestPacket : public BasePacket {
public:
    u_short player_id; // 세션 종료를 요청하는 클라이언트의 ID

    CS_EndSessionRequestPacket() {
        size = sizeof(CS_EndSessionRequestPacket);
        type = CS_END_SESSION_REQ;
        player_id = (u_short)-1;
    }

    CS_EndSessionRequestPacket(u_short id) {
        size = sizeof(CS_EndSessionRequestPacket);
        type = CS_END_SESSION_REQ;
        player_id = id;
    }

    void Encode() {
        size = htons(size);
        player_id = htons(player_id);
    }

    void Decode() {
        size = ntohs(size);
        player_id = ntohs(player_id);
    }

    void Log() const {
        printf("[CS_EndSessionRequestPacket] Type: %d, Size: %hu, PlayerID: %hu\n", type, size, player_id);
    }
};

// [S->C] 확정된 게임 맵 정보를 모든 클라이언트에게 전송하는 패킷
class SC_MapInfoPacket : public BasePacket {
public:
    Map mapInfo;

    SC_MapInfoPacket() {
        size = sizeof(SC_MapInfoPacket);
        type = SC_MAP_INFO;
        memset(&mapInfo, 0, sizeof(Map));
    }
    
    void Init(const Map& map) {
        mapInfo = map;
        // Ensure size and type are set correctly after initialization
        size = sizeof(SC_MapInfoPacket);
        type = SC_MAP_INFO;
    }

    void ConvertRectEndian(RECT& rect) {
        rect.left = htonl(rect.left);
        rect.top = htonl(rect.top);
        rect.right = htonl(rect.right);
        rect.bottom = htonl(rect.bottom);
    }

    void RestoreRectEndian(RECT& rect) {
        rect.left = ntohl(rect.left);
        rect.top = ntohl(rect.top);
        rect.right = ntohl(rect.right);
        rect.bottom = ntohl(rect.bottom);
    }

    void Encode() {
        size = htons(size);

        mapInfo.Map_width = htonl(mapInfo.Map_width);
        mapInfo.Map_height = htonl(mapInfo.Map_height);
        mapInfo.block_count = htonl(mapInfo.block_count);
        mapInfo.object_count = htonl(mapInfo.object_count);
        mapInfo.enemy_count = htonl(mapInfo.enemy_count);
        mapInfo.boss_count = htonl(mapInfo.boss_count);

        for (int i = 0; i < 160; ++i) {
            mapInfo.blocks[i].x = htonl(mapInfo.blocks[i].x);
            mapInfo.blocks[i].y = htonl(mapInfo.blocks[i].y);
            ConvertRectEndian(mapInfo.blocks[i].Block_rt);
        }

        for (int i = 0; i < 160; ++i) {
            mapInfo.objects[i].x = htonl(mapInfo.objects[i].x);
            mapInfo.objects[i].y = htonl(mapInfo.objects[i].y);
            mapInfo.objects[i].obj_type = (Object_type)htonl(mapInfo.objects[i].obj_type);
            ConvertRectEndian(mapInfo.objects[i].Obj_rt);
        }

        for (int i = 0; i < 32; ++i) {
            mapInfo.enemys[i].x = htonl(mapInfo.enemys[i].x);
            mapInfo.enemys[i].y = htonl(mapInfo.enemys[i].y);
            ConvertRectEndian(mapInfo.enemys[i].enemy_rect);
            mapInfo.enemys[i].direction = (Direction)htonl(mapInfo.enemys[i].direction);
            mapInfo.enemys[i].move_state = htonl(mapInfo.enemys[i].move_state);
            mapInfo.enemys[i].on_block = htonl(mapInfo.enemys[i].on_block);
        }
        
        mapInfo.boss.x = htonl(mapInfo.boss.x);
        mapInfo.boss.y = htonl(mapInfo.boss.y);
        ConvertRectEndian(mapInfo.boss.boss_rect);
        mapInfo.boss.life = htonl(mapInfo.boss.life);
        mapInfo.boss.attack_time = htonl(mapInfo.boss.attack_time);

        for (int i = 0; i < 3; ++i) {
            mapInfo.P_Start_Loc[i].x = htonl(mapInfo.P_Start_Loc[i].x);
            mapInfo.P_Start_Loc[i].y = htonl(mapInfo.P_Start_Loc[i].y);
        }
    }

    void Decode() {
        size = ntohs(size);

        mapInfo.Map_width = ntohl(mapInfo.Map_width);
        mapInfo.Map_height = ntohl(mapInfo.Map_height);
        mapInfo.block_count = ntohl(mapInfo.block_count);
        mapInfo.object_count = ntohl(mapInfo.object_count);
        mapInfo.enemy_count = ntohl(mapInfo.enemy_count);
        mapInfo.boss_count = ntohl(mapInfo.boss_count);

        for (int i = 0; i < 160; ++i) {
            mapInfo.blocks[i].x = ntohl(mapInfo.blocks[i].x);
            mapInfo.blocks[i].y = ntohl(mapInfo.blocks[i].y);
            RestoreRectEndian(mapInfo.blocks[i].Block_rt);
        }

        for (int i = 0; i < 160; ++i) {
            mapInfo.objects[i].x = ntohl(mapInfo.objects[i].x);
            mapInfo.objects[i].y = ntohl(mapInfo.objects[i].y);
            mapInfo.objects[i].obj_type = (Object_type)ntohl(mapInfo.objects[i].obj_type);
            RestoreRectEndian(mapInfo.objects[i].Obj_rt);
        }

        for (int i = 0; i < 32; ++i) {
            mapInfo.enemys[i].x = ntohl(mapInfo.enemys[i].x);
            mapInfo.enemys[i].y = ntohl(mapInfo.enemys[i].y);
            RestoreRectEndian(mapInfo.enemys[i].enemy_rect);
            mapInfo.enemys[i].direction = (Direction)ntohl(mapInfo.enemys[i].direction);
            mapInfo.enemys[i].move_state = ntohl(mapInfo.enemys[i].move_state);
            mapInfo.enemys[i].on_block = ntohl(mapInfo.enemys[i].on_block);
        }

        mapInfo.boss.x = ntohl(mapInfo.boss.x);
        mapInfo.boss.y = ntohl(mapInfo.boss.y);
        RestoreRectEndian(mapInfo.boss.boss_rect);
        mapInfo.boss.life = ntohl(mapInfo.boss.life);
        mapInfo.boss.attack_time = ntohl(mapInfo.boss.attack_time);

        for (int i = 0; i < 3; ++i) {
            mapInfo.P_Start_Loc[i].x = ntohl(mapInfo.P_Start_Loc[i].x);
            mapInfo.P_Start_Loc[i].y = ntohl(mapInfo.P_Start_Loc[i].y);
        }
    }

    void Log() const {
        printf("[SC_MapInfoPacket] Type: %d, Size: %hu\n", type, size);
        printf("  Map Width: %d, Map Height: %d\n", mapInfo.Map_width, mapInfo.Map_height);
        printf("  Block Count: %d\n", mapInfo.block_count);
        for (int i = 0; i < mapInfo.block_count; ++i) {
            printf("    Block %d: Pos=(%d, %d), Rect=(%ld, %ld, %ld, %ld)\n",
                   i, mapInfo.blocks[i].x, mapInfo.blocks[i].y, mapInfo.blocks[i].Block_rt.left, mapInfo.blocks[i].Block_rt.top, mapInfo.blocks[i].Block_rt.right, mapInfo.blocks[i].Block_rt.bottom);
        }

        printf("  Object Count: %d\n", mapInfo.object_count);
        for (int i = 0; i < mapInfo.object_count; ++i) {
            printf("    Object %d: Pos=(%d, %d), Type=%d, Rect=(%ld, %ld, %ld, %ld)\n",
                   i, mapInfo.objects[i].x, mapInfo.objects[i].y, mapInfo.objects[i].obj_type, mapInfo.objects[i].Obj_rt.left, mapInfo.objects[i].Obj_rt.top, mapInfo.objects[i].Obj_rt.right, mapInfo.objects[i].Obj_rt.bottom);
        }

        printf("  Enemy Count: %d\n", mapInfo.enemy_count);
        for (int i = 0; i < mapInfo.enemy_count; ++i) {
            printf("    Enemy %d: Pos=(%d, %d)\n", i, mapInfo.enemys[i].x, mapInfo.enemys[i].y);
        }
        
        printf("  Boss Count: %d\n", mapInfo.boss_count);
        if (mapInfo.boss_count > 0) {
            printf("    Boss: Pos=(%d, %d), Life=%d\n", mapInfo.boss.x, mapInfo.boss.y, mapInfo.boss.life);
        }

        printf("  Player Start Positions:\n");
        for (int i = 0; i < 3; ++i) {
            printf("    Player %d: (%d, %d)\n", i, mapInfo.P_Start_Loc[i].x, mapInfo.P_Start_Loc[i].y);
        }
    }
};

// [S->C] 특정 게임 이벤트를 알리는 패킷
class SC_EventPacket : public BasePacket {
public:
    E_EventType event_type; // 발생한 이벤트의 종류

    SC_EventPacket() {
        size = sizeof(SC_EventPacket);
        type = SC_EVENT;
        event_type = STAGE_CLEAR; // 기본값 설정
    }

    SC_EventPacket(E_EventType event) {
        size = sizeof(SC_EventPacket);
        type = SC_EVENT;
        event_type = event;
    }

    void Encode() {
        size = htons(size);
        event_type = (E_EventType)htonl(event_type); // enum도 int로 간주하여 변환
    }

    void Decode() {
        size = ntohs(size);
        event_type = (E_EventType)ntohl(event_type);
    }

    void Log() const {
        printf("[SC_EventPacket] Type: %d, Size: %hu, EventType: %d\n", type, size, event_type);
    }
};

// [S->C] 실시간 게임 상태 동기화 패킷
class SC_GameStatePacket : public BasePacket {
public:
    struct PlayerState {
        bool is_connected;
        Point pos;
        int life;
        int walk_state;
        int jump_state;
        int frame_counter;
        Direction dir;
    } players[3];

    struct EnemyState {
        bool is_alive;
        Point pos;
        Direction dir;
        int move_state;
    } enemies[32];

    struct BossState {
        bool is_active;
        Point pos;
        int life;
        int attack_time;
        Direction dir;
    } boss;

    SC_GameStatePacket() {
        size = sizeof(SC_GameStatePacket);
        type = SC_GAME_STATE;
        memset(players, 0, sizeof(players));
        memset(enemies, 0, sizeof(enemies));
        memset(&boss, 0, sizeof(boss));
    }

    void Encode() {
        size = htons(size);
        for (int i = 0; i < 3; ++i) {
            players[i].pos.x = htonl(players[i].pos.x);
            players[i].pos.y = htonl(players[i].pos.y);
            players[i].life = htonl(players[i].life);
            players[i].walk_state = htonl(players[i].walk_state);
            players[i].jump_state = htonl(players[i].jump_state);
            players[i].frame_counter = htonl(players[i].frame_counter);
            players[i].dir = (Direction) htonl(players[i].dir);
        }
        for (int i = 0; i < 32; ++i) {
            enemies[i].pos.x = htonl(enemies[i].pos.x);
            enemies[i].pos.y = htonl(enemies[i].pos.y);
            enemies[i].dir = (Direction) htonl(enemies[i].dir);
            enemies[i].move_state = htonl(enemies[i].move_state); // Fixed typo
        }
        boss.pos.x = htonl(boss.pos.x);
        boss.pos.y = htonl(boss.pos.y);
        boss.life = htonl(boss.life);
        boss.attack_time = htonl(boss.attack_time);
        boss.dir = (Direction) htonl(boss.dir);
    }

    void Decode() {
        size = ntohs(size);
        for (int i = 0; i < 3; ++i) {
            players[i].pos.x = ntohl(players[i].pos.x);
            players[i].pos.y = ntohl(players[i].pos.y);
            players[i].life = ntohl(players[i].life);
            players[i].walk_state = ntohl(players[i].walk_state);
            players[i].jump_state = ntohl(players[i].jump_state);
            players[i].frame_counter = ntohl(players[i].frame_counter);
            players[i].dir = (Direction)ntohl(players[i].dir);
        }
        for (int i = 0; i < 32; ++i) {
            enemies[i].pos.x = ntohl(enemies[i].pos.x);
            enemies[i].pos.y = ntohl(enemies[i].pos.y);
            enemies[i].dir = (Direction)ntohl(enemies[i].dir);
            enemies[i].move_state = ntohl(enemies[i].move_state); // Fixed typo
        }
        boss.pos.x = ntohl(boss.pos.x);
        boss.pos.y = ntohl(boss.pos.y);
        boss.life = ntohl(boss.life);
        boss.attack_time = ntohl(boss.attack_time);
        boss.dir = (Direction)ntohl(boss.dir);
    }

    void Log() const {
        printf("[SC_GameStatePacket] Type: %d, Size: %hu\n", type, size);
        for (int i = 0; i < 3; ++i) {
            if (!players[i].is_connected) continue;
            printf("  Player %d: Pos=(%d, %d), Life=%d, Walk=%d, Jump=%d, Frame=%d, Dir=%d\n",
                   i, players[i].pos.x, players[i].pos.y, players[i].life,
                   players[i].walk_state, players[i].jump_state, players[i].frame_counter, players[i].dir);
        }
        for (int i = 0; i < 32; ++i) {
            if (enemies[i].is_alive) {
                printf("  Enemy %d: Pos=(%d, %d), Dir=%d, MoveState=%d\n",
                       i, enemies[i].pos.x, enemies[i].pos.y, enemies[i].dir, enemies[i].move_state);
            }
        }
        if (boss.is_active) {
            printf("  Boss: Pos=(%d, %d), Life=%d, AttackTime=%d, Dir=%d\n",
                   boss.pos.x, boss.pos.y, boss.life, boss.attack_time, boss.dir);
        }
    }
};

#pragma pack(pop)

#endif // PACKETS_H