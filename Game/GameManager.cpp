#include "GameManager.h"


Player Make_Player(int x, int y) {
	Player newPlayer;
	newPlayer.x = x;
	newPlayer.y = y;
	newPlayer.LEFT = newPlayer.RIGHT = newPlayer.UP = newPlayer.DOWN = false;
	newPlayer.is_in_air = false;
	newPlayer.jump_count = 0;
	newPlayer.player_rt = { newPlayer.x - Size, newPlayer.y - Size, newPlayer.x + Size, newPlayer.y + Size };
	newPlayer.player_life = 3;
	return newPlayer;
}

Enemy Make_Enemy(int start_x, int start_y, int block_num) {
	Enemy newEnemy;
	newEnemy.x = start_x;
	newEnemy.y = start_y;
	newEnemy.on_block = block_num;
	newEnemy.enemy_rect = { newEnemy.x - Size, newEnemy.y - Size, newEnemy.x + Size, newEnemy.y + Size };
	newEnemy.direction = LEFT;
	newEnemy.is_alive = true;
	return newEnemy;
}

Map init_map(RECT Desk_rect, Player* player, int map_num) {
	Map new_map;
	int randomnum;
	new_map.block_count = new_map.object_count = new_map.enemy_count = new_map.boss_count = 0;
	switch (map_num)
	{
	case 0://0번 맵

		//맵 초기화 및 생성

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
		new_map.P_start_x = Desk_rect.left + 64;
		new_map.P_start_y = Desk_rect.bottom - 128;
		break;
	case 1://1번 맵

		//맵 초기화 및 생성

		//맵 블럭 초기화
		new_map.blocks[new_map.block_count].x = Desk_rect.right / 2;
		new_map.blocks[new_map.block_count].y = (Desk_rect.bottom - 100);
		new_map.blocks[new_map.block_count].Block_rt = { Desk_rect.left,Desk_rect.bottom - 128,Desk_rect.right,Desk_rect.bottom };
		new_map.block_count++;

		new_map.blocks[new_map.block_count].x = Desk_rect.left + 448;
		new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 96;
		new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 64,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 64,new_map.blocks[new_map.block_count].y + 32 };
		new_map.block_count++;

		for (int i = 0; i < (Desk_rect.bottom - 192) / 240; i++)
		{
			new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 320;
			new_map.blocks[new_map.block_count].y = new_map.blocks[new_map.block_count - 1].y - 192;
			new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 64,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 64,new_map.blocks[new_map.block_count].y + 32 };
			new_map.block_count++;
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
		
		new_map.objects[new_map.object_count].x = new_map.blocks[new_map.block_count - 1].Block_rt.right - Size;
		new_map.objects[new_map.object_count].y = new_map.blocks[new_map.block_count - 1].Block_rt.top - 8;
		new_map.objects[new_map.object_count].obj_type = Flag;
		new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
		new_map.object_count++;
		
		//플레이어 생성
		new_map.P_start_x = Desk_rect.left + 64;
		new_map.P_start_y = Desk_rect.bottom - 128;
		break;
	case 2://2번 맵

		//맵 초기화 및 생성

		//맵 블럭 초기화
		new_map.blocks[new_map.block_count].x = Desk_rect.right / 2;
		new_map.blocks[new_map.block_count].y = (Desk_rect.bottom - 100);
		new_map.blocks[new_map.block_count].Block_rt = { Desk_rect.left,Desk_rect.bottom - 128,Desk_rect.right,Desk_rect.bottom };
		new_map.block_count++;

		new_map.blocks[new_map.block_count].x = Desk_rect.left + 448;
		new_map.blocks[new_map.block_count].y = new_map.blocks[0].Block_rt.top - 96;
		new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 64,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 64,new_map.blocks[new_map.block_count].y + 32 };
		new_map.block_count++;

		for (int i = 0; i < (Desk_rect.bottom - 192) / 240; i++)
		{
			new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 320;
			new_map.blocks[new_map.block_count].y = new_map.blocks[new_map.block_count - 1].y + (rand() % 640 - 320);
			while (new_map.blocks[new_map.block_count].y >= new_map.blocks[0].Block_rt.top)
			{
				new_map.blocks[new_map.block_count].y = new_map.blocks[new_map.block_count - 1].y + (rand() % 640 - 320);
			}
			new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 64,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 64,new_map.blocks[new_map.block_count].y + 32 };
			new_map.block_count++;
			
			new_map.enemys[new_map.enemy_count] = Make_Enemy(new_map.blocks[new_map.block_count - 1].x, new_map.blocks[new_map.block_count - 1].Block_rt.top - Size, new_map.block_count - 1);
			new_map.enemy_count++;
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

		new_map.objects[new_map.object_count].x = new_map.blocks[new_map.block_count - 1].Block_rt.right - Size;
		new_map.objects[new_map.object_count].y = new_map.blocks[new_map.block_count - 1].Block_rt.top - 8;
		new_map.objects[new_map.object_count].obj_type = Flag;
		new_map.objects[new_map.object_count].Obj_rt = { (new_map.objects[new_map.object_count].x - Size),(new_map.objects[new_map.object_count].y - Size),(new_map.objects[new_map.object_count].x + Size),(new_map.objects[new_map.object_count].y + Size) };
		new_map.object_count++;

		//플레이어 생성
		new_map.P_start_x = Desk_rect.left + 64;
		new_map.P_start_y = Desk_rect.bottom - 128;
		break;
	case 3://3번 맵(보스전)

		//맵 초기화 및 생성

		//맵 블럭 초기화

		new_map.blocks[new_map.block_count].x = Desk_rect.left + 100;
		new_map.blocks[new_map.block_count].y = Desk_rect.bottom - 200;
		new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 160,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 160,new_map.blocks[new_map.block_count].y + 32 };
		new_map.block_count++;

		for (int i = 0; i < ((Desk_rect.right - 260) / 480); i++)
		{
			new_map.blocks[new_map.block_count].x = new_map.blocks[new_map.block_count - 1].x + 480;
			new_map.blocks[new_map.block_count].y = Desk_rect.bottom - ((rand() % 256) + 160);
			new_map.blocks[new_map.block_count].Block_rt = { new_map.blocks[new_map.block_count].x - 160,new_map.blocks[new_map.block_count].y - 32,new_map.blocks[new_map.block_count].x + 160,new_map.blocks[new_map.block_count].y + 32 };
			new_map.block_count++;
		}

		new_map.enemys[new_map.enemy_count] = Make_Enemy(new_map.blocks[1].x, new_map.blocks[1].Block_rt.top - Size, 1);
		new_map.enemy_count++;

		//보스 초기화
		new_map.boss.x = Desk_rect.right - 160;
		new_map.boss.y = Desk_rect.bottom - 320;
		new_map.boss.life = 3;
		new_map.boss.boss_rect = { new_map.boss.x - 160,new_map.boss.y - 160,new_map.boss.x + 160,new_map.boss.y + 160 };
		new_map.boss_count++;
		new_map.boss.attack_time = 480;

		//플레이어 생성
		new_map.P_start_x = new_map.blocks[0].x;
		new_map.P_start_y = new_map.blocks[0].Block_rt.top - Size;
		break;
	default:
		//플레이어 생성
		new_map.P_start_x = Desk_rect.right/2;
		new_map.P_start_y = Desk_rect.bottom/2;
		break;
	}

	*player = Make_Player(new_map.P_start_x, new_map.P_start_y);
	return new_map;
}