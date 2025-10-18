#pragma once
#include <Windows.h>
#include "resource.h"
#define Size 24

//구조체 선언 및 공용 함수등을 정의하는 헤더파일

typedef struct {
	int x, y, jump_count, jump_time;
	int Walk_state, Jump_state;
	bool is_in_air;
	bool LEFT, RIGHT, UP, DOWN;//UP : 점프 DOWN : 찍기공격 중
	RECT player_rt;
	int player_life;
	int frame_counter; // 프레임 카운터 추가
}Player;

enum Direction {
	LEFT,
	RIGHT
};

typedef struct {
	int x, y;
	RECT enemy_rect;
	bool is_alive;
	enum Direction direction;
	int move_state;
	int on_block;
}Enemy;

typedef struct {
	int x, y;
	RECT boss_rect;
	int life;

	int attack_time;
}Boss;

typedef struct {
	int x, y;
	RECT Block_rt;
}Block;

enum Object_type {
	Spike, Flag
};

typedef struct {
	int x, y;
	Object_type obj_type;
	RECT Obj_rt;
}Object;

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

	int P_start_x, P_start_y;
}Map;

Map init_map(RECT Desk_rect, Player* player, int map_num);

Player Make_Player(int x, int y);

Enemy Make_Enemy(int start_x, int start_y, int block_num);