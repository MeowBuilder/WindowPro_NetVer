#include "Player&Enemy.h"


//플레이어 함수

void Player_Draw(HDC* mdc, HDC* resourcedc, HBITMAP Tino_bitmap, Player player) {
	// 애니메이션 프레임 선택
	int frame = (player.frame_counter / 30) % 2; 

	SelectObject(*resourcedc, Tino_bitmap);
	if ( player.Walk_state == 0) {
		if (player.Jump_state == 0) {
			TransparentBlt(*mdc, player.x - Size, player.y - Size, (Size * 2), (Size * 2), *resourcedc, 0, 0, 160, 140, RGB(0, 0, 255));
		}
		else if (player.Jump_state == 1) {
			TransparentBlt(*mdc, player.x - Size, player.y - Size, (Size * 2), (Size * 2), *resourcedc, 0, 480, 160, 140, RGB(0, 0, 255));
		}
	}
	else if (player.LEFT ) {
		if (player.Jump_state == 0 || player.Walk_state == 1) {
			int spriteY = (frame == 0) ? 160 : 320;
			TransparentBlt(*mdc, player.x - Size, player.y - Size, (Size * 2), (Size * 2), *resourcedc, 160, spriteY, 140, 140, RGB(0, 0, 255));
		}
	}
	else if (player.RIGHT ) {
		if (player.Jump_state == 0 || player.Walk_state == 2) {
			int spriteY = (frame == 0) ? 160 : 320;
			TransparentBlt(*mdc, player.x - Size, player.y - Size, (Size * 2), (Size * 2), *resourcedc, 0, spriteY, 150, 140, RGB(0, 0, 255));
		}
	}
	

	// 플레이어 머리 위에 목숨 수만큼 체력 바 그리기
	int lifeBarX = player.x - Size;
	int lifeBarY = player.y - (Size * 2);
	int lifeBarHeight = 10;
	int lifeBarSpacing = 3;

	for (int i = 0; i < player.player_life; ++i) {
		int lifeBarCenterX = lifeBarX + (i * (lifeBarSpacing + lifeBarHeight)) + lifeBarHeight / 2;
		int lifeBarCenterY = lifeBarY + lifeBarHeight / 2;
		int radius = lifeBarHeight / 2;
		HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 0));
		SelectObject(*mdc, hBrush);
		Ellipse(*mdc, lifeBarCenterX - radius, lifeBarCenterY - radius, lifeBarCenterX + radius, lifeBarCenterY + radius);
		DeleteObject(hBrush);
	}
}

void Player_Move(Player *player,RECT window_rect) {
	if ((*player).UP) {
		if ((*player).jump_time < 25)
		{
			(*player).y -= 3;
		}
		else
		{
			(*player).jump_time = 0;
			(*player).UP = false;

		}
		(*player).jump_time++;
		(*player).Jump_state = 1;
	}
	else {
		(*player).Jump_state = 0;
	}

	if ((*player).DOWN)
	{
		(*player).y += 10;
	}

	if ((*player).LEFT) {
		if (!((*player).x - Size <= window_rect.left))
			(*player).x -= 5;
		(*player).Walk_state = 1; // 왼쪽으로 이동 중
	}
	else if ((*player).RIGHT) {
		if (!((*player).x + Size >= window_rect.right - Size))
			(*player).x += 5;
		(*player).Walk_state = 2; // 오른쪽으로 이동 중
	}
	else {
		(*player).Walk_state = 0; // 가만히 있는 상태를 나타내는 값으로 설정
	}

	// 프레임 카운터 증가 (프레임 기반 애니메이션)
	(*player).frame_counter++;
	if ((*player).frame_counter >= 30){
		(*player).frame_counter = 0;
	}


	if ((*player).is_in_air)
	{
		if (is_on_Wall((*player), window_rect))
		{
			(*player).UP = false;
			if (((*player).x - Size <= window_rect.left))
			{
				if ((*player).LEFT)
				{
					(*player).y++;
					(*player).jump_count = 2;
				}
				else if (!(*player).UP) (*player).y += 5;


			}

			if (((*player).x + Size >= window_rect.right - Size))
			{
				if ((*player).RIGHT)
				{
					(*player).y++;
					(*player).jump_count = 2;
				}
				else if (!(*player).UP) (*player).y += 5;
			}
			
			
		}
		else
		{
			if (!(*player).UP) (*player).y += 5;
		}
	}

	(*player).is_in_air = true;

	(*player).player_rt = { (*player).x - Size, (*player).y - Size, (*player).x + Size, (*player).y + Size };
}

bool is_on_Wall(Player player, RECT wnd_rt) {
	return (player.x - Size <= wnd_rt.left) || (player.x + Size >= wnd_rt.right - Size);
}


//적 함수

void Move_Enemy(Enemy* enemy, Block Enemy_on_block, int speed) {
	if (enemy->is_alive)
	{
		if (enemy->direction == LEFT) {
			if (enemy->x - Size <= Enemy_on_block.Block_rt.left)
			{
				enemy->direction = RIGHT;
			}
			enemy->x -= speed;
		}
		else if (enemy->direction == RIGHT) {
			if (enemy->x + Size >= Enemy_on_block.Block_rt.right)
			{
				enemy->direction = LEFT;
			}
			enemy->x += speed;
		}
	}
}

void Update_Enemy_rect(Enemy* enemy) {
	(*enemy).enemy_rect = { (*enemy).x - Size, (*enemy).y - Size, (*enemy).x + Size, (*enemy).y + Size };
}