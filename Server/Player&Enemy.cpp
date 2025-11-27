#include "Player&Enemy.h"

// -------------------------------------
// 플레이어 이동 (Server authoritative)
// -------------------------------------
void Player_Move(Player* player, RECT window_rect)
{
    // --- 점프 ---
    if (player->UP)
    {
        if (player->jump_time < 25)
        {
            player->y -= 3;
        }
        else
        {
            // 점프 종료
            player->jump_time = 0;
            player->UP = false;
        }

        player->jump_time++;
        player->Jump_state = 1;
    }
    else
    {
        player->Jump_state = 0;
    }

    // --- 아래로 빠르게 떨어지기 ---
    if (player->DOWN)
        player->y += 10;

    // --- 좌/우 이동 ---
    if (player->LEFT)
    {
        if (player->x - Size > window_rect.left)
            player->x -= 5;

        player->Walk_state = 1;
    }
    else if (player->RIGHT)
    {
        if (player->x + Size < window_rect.right - Size)
            player->x += 5;

        player->Walk_state = 2;
    }
    else
    {
        player->Walk_state = 0;
    }

    // --- 중력 ---
    if (player->is_in_air)
    {
        if (is_on_Wall(*player, window_rect))
        {
            // 벽에서 미끄러지기
            player->UP = false;

            if (player->x - Size <= window_rect.left)
            {
                if (player->LEFT)
                {
                    player->y++;
                    player->jump_count = 2;
                }
                else if (!player->UP)
                {
                    player->y += 5;
                }
            }

            if (player->x + Size >= window_rect.right - Size)
            {
                if (player->RIGHT)
                {
                    player->y++;
                    player->jump_count = 2;
                }
                else if (!player->UP)
                {
                    player->y += 5;
                }
            }
        }
        else
        {
            if (!player->UP)
                player->y += 5;
        }
    }

    // 항상 공중 판정 (바닥 충돌 로직은 서버 별도 처리 가능)
    player->is_in_air = true;

    // 사각형 업데이트
    player->player_rt = {
        player->x - Size, player->y - Size,
        player->x + Size, player->y + Size
    };
}

// -------------------------------------
// 벽 충돌 판정
// -------------------------------------
bool is_on_Wall(const Player& player, RECT wnd_rt)
{
    return (player.x - Size <= wnd_rt.left) ||
        (player.x + Size >= wnd_rt.right - Size);
}

// -------------------------------------
// Enemy 이동 (단순 좌우 왕복로직)
// -------------------------------------
void Move_Enemy(Enemy* enemy, const Block& platform, int speed)
{
    if (!enemy->is_alive)
        return;

    if (enemy->direction == LEFT)
    {
        if (enemy->x - Size <= platform.Block_rt.left)
            enemy->direction = RIGHT;

        enemy->x -= speed;
    }
    else
    {
        if (enemy->x + Size >= platform.Block_rt.right)
            enemy->direction = LEFT;

        enemy->x += speed;
    }
}

// -------------------------------------
// Enemy 사각형 갱신
// -------------------------------------
void Update_Enemy_rect(Enemy* enemy)
{
    enemy->enemy_rect = {
        enemy->x - Size, enemy->y - Size,
        enemy->x + Size, enemy->y + Size
    };
}
