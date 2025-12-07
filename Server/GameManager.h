#pragma once
#include <windows.h>

// ==== 서버 전용 Point ==== //
struct Point {
    int x;
    int y;
};

#define Size 24

// ==== Player ==== //
typedef struct {
    u_short player_id;
    bool is_connected;

    int x, y;
    int jump_count, jump_time;

    int Walk_state;   // 0 idle / 1 left / 2 right
    int Jump_state;   // 0 ground / 1 jump
    bool is_in_air;

    bool LEFT, RIGHT, UP, DOWN;

    RECT player_rt;

    int player_life;
    int frame_counter;

    bool window_move;
} Player;

// ==== Direction ==== //
enum Direction {
    LEFT,
    RIGHT
};

// ==== Enemy ==== //
typedef struct {
    int x, y;
    RECT enemy_rect;
    bool is_alive;
    Direction direction;
    int move_state;
    int on_block;
} Enemy;

// ==== Boss ==== //
typedef struct {
    int x, y;
    RECT boss_rect;
    int life;
    int attack_time;
} Boss;

// ==== Block ==== //
typedef struct {
    int x, y;
    RECT Block_rt;
} Block;

// ==== Object ==== //
enum Object_type {
    Spike,
    Flag
};

typedef struct {
    int x, y;
    Object_type obj_type;
    RECT Obj_rt;
} Object;

// ==== Map ==== //
typedef struct {
    int Map_width, Map_height;

    Block blocks[160];
    int block_count;

    Object objects[160];
    int object_count;

    Enemy enemys[32];
    int enemy_count;

    Boss boss;
    int boss_count;

    Point P_Start_Loc[3];

} Map;

// ==== 함수 선언 ==== //
Map init_map(RECT Desk_rect, Player* player, int map_num);

Player Make_Player(int x, int y);

Enemy Make_Enemy(int start_x, int start_y, int block_num);