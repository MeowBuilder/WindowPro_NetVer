#pragma once
#include "GameManager.h"


void Player_Move(Player* player, RECT window_rect);

void Player_Draw(HDC* mdc, HDC* resourcedc, HBITMAP Character_bitmap, Player player);

bool is_on_Wall(Player player, RECT wnd_rt);

void Move_Enemy(Enemy* enemy, Block Enemy_on_block, int speed);

void Update_Enemy_rect(Enemy* enemy);