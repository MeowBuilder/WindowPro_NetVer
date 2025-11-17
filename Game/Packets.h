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

// --- 사용자 정의 자료구조 ---
// 프로젝트 계획서에 정의된 좌표 구조체
struct Point
{
    int x;
    int y;
};
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
    int block_count;
    Block blocks[160];
    int object_count;
    Object objects[160];
    int enemy_spawn_count;
    Point enemy_spawns[32];
    Point player_start_pos[3];


    CS_UploadMapPacket() {
        size = sizeof(CS_UploadMapPacket);
        type = CS_UPLOAD_MAP;
        block_count = 0;
        object_count = 0;
        enemy_spawn_count = 0;
        memset(blocks, 0, sizeof(blocks));
        memset(objects, 0, sizeof(objects));
        memset(enemy_spawns, 0, sizeof(enemy_spawns));
        memset(player_start_pos, 0, sizeof(player_start_pos));
    }

    void Init(Map& map) {
        size = sizeof(CS_UploadMapPacket);
        type = SC_MAP_INFO;
        block_count = map.block_count;
        object_count = map.object_count;
        enemy_spawn_count = map.enemy_count;
        memset(blocks, 0, sizeof(blocks));
        memset(objects, 0, sizeof(objects));
        memset(enemy_spawns, 0, sizeof(enemy_spawns));
        memset(player_start_pos, 0, sizeof(player_start_pos));
        for (int i = 0; i < block_count; i++)
        {
            blocks[i] = map.blocks[i];
        }
        for (int i = 0; i < object_count; i++)
        {
            objects[i] = map.objects[i];
        }
        for (int i = 0; i < enemy_spawn_count; i++)
        {
            enemy_spawns[i] = Point(map.enemys[i].x, map.enemys[i].y);
        }
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
        block_count = htonl(block_count);
        for (int i = 0; i < 160; ++i) {
            blocks[i].x = htonl(blocks[i].x);
            blocks[i].y = htonl(blocks[i].y);
            ConvertRectEndian(blocks[i].Block_rt);
        }
        object_count = htonl(object_count);
        for (int i = 0; i < 160; ++i) {
            objects[i].x = htonl(objects[i].x);
            objects[i].y = htonl(objects[i].y);
            objects[i].obj_type = (Object_type) htonl(objects[i].obj_type);
            ConvertRectEndian(objects[i].Obj_rt);
        }
        enemy_spawn_count = htonl(enemy_spawn_count);
        for (int i = 0; i < 32; ++i) {
            enemy_spawns[i].x = htonl(enemy_spawns[i].x);
            enemy_spawns[i].y = htonl(enemy_spawns[i].y);
        }
        for (int i = 0; i < 3; ++i) {
            player_start_pos[i].x = htonl(player_start_pos[i].x);
            player_start_pos[i].y = htonl(player_start_pos[i].y);
        }
    }

    void Decode() {
        size = ntohs(size);
        block_count = ntohl(block_count);
        for (int i = 0; i < 160; ++i) {
            blocks[i].x = ntohl(blocks[i].x);
            blocks[i].y = ntohl(blocks[i].y);
            RestoreRectEndian(blocks[i].Block_rt);
        }
        object_count = ntohl(object_count);
        for (int i = 0; i < 160; ++i) {
            objects[i].x = ntohl(objects[i].x);
            objects[i].y = ntohl(objects[i].y);
            objects[i].obj_type = (Object_type)ntohl(objects[i].obj_type);
            RestoreRectEndian(objects[i].Obj_rt);
        }
        enemy_spawn_count = ntohl(enemy_spawn_count);
        for (int i = 0; i < 32; ++i) {
            enemy_spawns[i].x = ntohl(enemy_spawns[i].x);
            enemy_spawns[i].y = ntohl(enemy_spawns[i].y);
        }
        for (int i = 0; i < 3; ++i) {
            player_start_pos[i].x = ntohl(player_start_pos[i].x);
            player_start_pos[i].y = ntohl(player_start_pos[i].y);
        }
    }

    void Log() const {
        printf("[CS_UploadMapPacket] Type: %d, Size: %hu\n", type, size);
        printf("  Block Count: %d\n", block_count);
        for (int i = 0; i < block_count; ++i) { // Log all blocks
            printf("    Block %d: Pos=(%d, %d), Rect=(%d, %d, %d, %d)\n",
                   i, blocks[i].x, blocks[i].y, blocks[i].Block_rt.left, blocks[i].Block_rt.top, blocks[i].Block_rt.right, blocks[i].Block_rt.bottom);
        }

        printf("  Object Count: %d\n", object_count);
        for (int i = 0; i < object_count; ++i) { // Log all objects
            printf("    Object %d: Pos=(%d, %d), Type=%d, Rect=(%d, %d, %d, %d)\n",
                   i, objects[i].x, objects[i].y, objects[i].obj_type, objects[i].Obj_rt.left, objects[i].Obj_rt.top, objects[i].Obj_rt.right, objects[i].Obj_rt.bottom);
        }

        printf("  Enemy Spawn Count: %d\n", enemy_spawn_count);
        for (int i = 0; i < enemy_spawn_count; ++i) { // Log all enemy spawns
            printf("    Enemy Spawn %d: Pos=(%d, %d)\n", i, enemy_spawns[i].x, enemy_spawns[i].y);
        }

        printf("  Player Start Positions:\n");
        for (int i = 0; i < 3; ++i) {
            printf("    Player %d: (%d, %d)\n", i, player_start_pos[i].x, player_start_pos[i].y);
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

// [S->C] 확정된 게임 맵 정보를 모든 클라이언트에게 전송하는 패킷
class SC_MapInfoPacket : public BasePacket {
public:
    int block_count;
    Block blocks[160];
    int object_count;
    Object objects[160];
    int enemy_spawn_count;
    Point enemy_spawns[32];
    Point player_start_pos[3];

    SC_MapInfoPacket() {
        size = sizeof(SC_MapInfoPacket);
        type = SC_MAP_INFO;
        block_count = 0;
        object_count = 0;
        enemy_spawn_count = 0;
        memset(blocks, 0, sizeof(blocks));
        memset(objects, 0, sizeof(objects));
        memset(enemy_spawns, 0, sizeof(enemy_spawns));
        memset(player_start_pos, 0, sizeof(player_start_pos));
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
        block_count = htonl(block_count);
        for (int i = 0; i < 160; ++i) {
            blocks[i].x = htonl(blocks[i].x);
            blocks[i].y = htonl(blocks[i].y);
            ConvertRectEndian(blocks[i].Block_rt);
        }
        object_count = htonl(object_count);
        for (int i = 0; i < 160; ++i) {
            objects[i].x = htonl(objects[i].x);
            objects[i].y = htonl(objects[i].y);
            objects[i].obj_type = (Object_type) htonl(objects[i].obj_type);
            ConvertRectEndian(objects[i].Obj_rt);
        }
        enemy_spawn_count = htonl(enemy_spawn_count);
        for (int i = 0; i < 32; ++i) {
            enemy_spawns[i].x = htonl(enemy_spawns[i].x);
            enemy_spawns[i].y = htonl(enemy_spawns[i].y);
        }
        for (int i = 0; i < 3; ++i) {
            player_start_pos[i].x = htonl(player_start_pos[i].x);
            player_start_pos[i].y = htonl(player_start_pos[i].y);
        }
    }

    void Decode() {
        size = ntohs(size);
        block_count = ntohl(block_count);
        for (int i = 0; i < 160; ++i) {
            blocks[i].x = ntohl(blocks[i].x);
            blocks[i].y = ntohl(blocks[i].y);
            RestoreRectEndian(blocks[i].Block_rt);
        }
        object_count = ntohl(object_count);
        for (int i = 0; i < 160; ++i) {
            objects[i].x = ntohl(objects[i].x);
            objects[i].y = ntohl(objects[i].y);
            objects[i].obj_type = (Object_type)ntohl(objects[i].obj_type);
            RestoreRectEndian(objects[i].Obj_rt);
        }
        enemy_spawn_count = ntohl(enemy_spawn_count);
        for (int i = 0; i < 32; ++i) {
            enemy_spawns[i].x = ntohl(enemy_spawns[i].x);
            enemy_spawns[i].y = ntohl(enemy_spawns[i].y);
        }
        for (int i = 0; i < 3; ++i) {
            player_start_pos[i].x = ntohl(player_start_pos[i].x);
            player_start_pos[i].y = ntohl(player_start_pos[i].y);
        }
    }

    void Log() const {
        printf("[SC_MapInfoPacket] Type: %d, Size: %hu\n", type, size);
        printf("  Block Count: %d\n", block_count);
        for (int i = 0; i < block_count; ++i) { // Log all blocks
            printf("    Block %d: Pos=(%d, %d), Rect=(%d, %d, %d, %d)\n",
                   i, blocks[i].x, blocks[i].y, blocks[i].Block_rt.left, blocks[i].Block_rt.top, blocks[i].Block_rt.right, blocks[i].Block_rt.bottom);
        }

        printf("  Object Count: %d\n", object_count);
        for (int i = 0; i < object_count; ++i) { // Log all objects
            printf("    Object %d: Pos=(%d, %d), Type=%d, Rect=(%d, %d, %d, %d)\n",
                   i, objects[i].x, objects[i].y, objects[i].obj_type, objects[i].Obj_rt.left, objects[i].Obj_rt.top, objects[i].Obj_rt.right, objects[i].Obj_rt.bottom);
        }

        printf("  Enemy Spawn Count: %d\n", enemy_spawn_count);
        for (int i = 0; i < enemy_spawn_count; ++i) { // Log all enemy spawns
            printf("    Enemy Spawn %d: Pos=(%d, %d)\n", i, enemy_spawns[i].x, enemy_spawns[i].y);
        }

        printf("  Player Start Positions:\n");
        for (int i = 0; i < 3; ++i) {
            printf("    Player %d: (%d, %d)\n", i, player_start_pos[i].x, player_start_pos[i].y);
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