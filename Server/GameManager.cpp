#include "GameManager.h"

// ========================================
// Player 생성
// ========================================
Player Make_Player(int x, int y)
{
    Player p{};
    p.player_id = 0;
    p.is_connected = false;

    p.x = x;
    p.y = y;

    p.jump_count = 0;
    p.jump_time = 0;

    p.Walk_state = 0;
    p.Jump_state = 0;
    p.is_in_air = false;

    p.LEFT = p.RIGHT = p.UP = p.DOWN = false;

    p.player_rt = { x - Size, y - Size, x + Size, y + Size };

    p.player_life = 3;
    p.frame_counter = 0;

    return p;
}

// ========================================
// Enemy 생성
// ========================================
Enemy Make_Enemy(int start_x, int start_y, int block_num)
{
    Enemy e{};
    e.x = start_x;
    e.y = start_y;

    e.enemy_rect = {
        start_x - Size,
        start_y - Size,
        start_x + Size,
        start_y + Size
    };

    e.is_alive = true;
    e.direction = LEFT;
    e.move_state = 0;
    e.on_block = block_num;

    return e;
}

// ========================================
// 기본 맵 생성 (서버 전용)
// ========================================
Map init_map(RECT Desk_rect, Player* player, int map_num)
{
    Map map{};
    map.Map_width = Desk_rect.right;
    map.Map_height = Desk_rect.bottom;

    map.block_count = 0;
    map.object_count = 0;
    map.enemy_count = 0;
    map.boss_count = 0;

    int r = 0;

    switch (map_num)
    {
    case 0:
    {
        // ----------------------------------
        // 바닥 플랫폼(고정값)
        // ----------------------------------
        map.blocks[0].x = Desk_rect.right / 2;
        map.blocks[0].y = Desk_rect.bottom - 100;
        map.blocks[0].Block_rt = {
            Desk_rect.left,
            Desk_rect.bottom - 128,
            Desk_rect.right,
            Desk_rect.bottom
        };
        map.block_count++;

        // ----------------------------------
        // 첫 번째 작은 발판
        // ----------------------------------
        map.blocks[1].x = Desk_rect.left + 448;
        map.blocks[1].y = map.blocks[0].Block_rt.top - 96;

        map.blocks[1].Block_rt = {
            map.blocks[1].x - 128,
            map.blocks[1].y - 32,
            map.blocks[1].x + 128,
            map.blocks[1].y + 32
        };
        map.block_count++;

        // ----------------------------------
        // 랜덤 플랫폼 생성
        // ----------------------------------
        for (int i = 0; i < (Desk_rect.right - 896) / 384; i++)
        {
            r = rand() % 2;

            int nextX = map.blocks[map.block_count - 1].x + 384;
            int nextY = (r == 0)
                ? map.blocks[0].Block_rt.top - 192
                : map.blocks[0].Block_rt.top - 96;

            map.blocks[map.block_count].x = nextX;
            map.blocks[map.block_count].y = nextY;

            map.blocks[map.block_count].Block_rt = {
                nextX - 128, nextY - 32,
                nextX + 128, nextY + 32
            };

            map.block_count++;
        }

        // ----------------------------------
        // 스파이크 1개
        // ----------------------------------
        map.objects[0].x = Desk_rect.left + 320;
        map.objects[0].y = map.blocks[0].Block_rt.top - 8;
        map.objects[0].obj_type = Spike;

        map.objects[0].Obj_rt = {
            map.objects[0].x - Size,
            map.objects[0].y - Size,
            map.objects[0].x + Size,
            map.objects[0].y + Size
        };
        map.object_count++;

        // ----------------------------------
        // 스파이크 여러 개
        // ----------------------------------
        for (int i = 0; i < (Desk_rect.right - 640) / (Size * 2); i++)
        {
            int nextX = map.objects[map.object_count - 1].x + (Size * 2);

            map.objects[map.object_count].x = nextX;
            map.objects[map.object_count].y = map.blocks[0].Block_rt.top - 8;
            map.objects[map.object_count].obj_type = Spike;

            map.objects[map.object_count].Obj_rt = {
                nextX - Size, map.objects[0].y - Size,
                nextX + Size, map.objects[0].y + Size
            };

            map.object_count++;
        }

        // ----------------------------------
        // 깃발
        // ----------------------------------
        int fx = map.objects[map.object_count - 1].x + 48;
        int fy = map.blocks[0].Block_rt.top - 8;

        map.objects[map.object_count].x = fx;
        map.objects[map.object_count].y = fy;
        map.objects[map.object_count].obj_type = Flag;

        map.objects[map.object_count].Obj_rt = {
            fx - Size, fy - Size,
            fx + Size, fy + Size
        };
        map.object_count++;

        // ----------------------------------
        // 플레이어 초기 위치
        // ----------------------------------
        map.P_Start_Loc[0] = {
            Desk_rect.left + 64,
            Desk_rect.bottom - 128
        };

        break;
    }

    default:
        map.P_Start_Loc[0] = { Desk_rect.right / 2, Desk_rect.bottom / 2 };
        break;
    }

    // ----------------------------------
    // 실제 플레이어 구조체 생성
    // ----------------------------------
    *player = Make_Player(map.P_Start_Loc[0].x, map.P_Start_Loc[0].y);

    return map;
}