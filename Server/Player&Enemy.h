#pragma once
#include "GameManager.h"

// ----------------------
// 서버용 순수 이동/로직 함수들
// ----------------------

// 플레이어 이동 (입력 기반 + 중력 + 벽/바닥 판정)
void Player_Move(Player* player, RECT window_rect);

// 플레이어 벽 충돌 판정
bool is_on_Wall(const Player& player, RECT wnd_rt);

// Enemy 이동 (단순 좌우 패턴)
void Move_Enemy(Enemy* enemy, const Block& Enemy_on_block, int speed);

// Enemy 사각형 갱신
void Update_Enemy_rect(Enemy* enemy);