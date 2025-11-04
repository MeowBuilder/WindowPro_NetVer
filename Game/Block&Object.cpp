#include "Block&Object.h"

void Draw_Map(HDC* mdc, HDC* resourcedc, HBITMAP Object_bitmap, HBITMAP Platforms_bitmap, HBITMAP Enemy_bitmap, HBITMAP Enemy_rv_bitmap, Map map) {
	SelectObject(*resourcedc, Platforms_bitmap);
	for (int i = 0; i < map.block_count; i++)
	{
		TransparentBlt(*mdc, map.blocks[i].Block_rt.left, map.blocks[i].Block_rt.top, map.blocks[i].Block_rt.right - map.blocks[i].Block_rt.left, map.blocks[i].Block_rt.bottom - map.blocks[i].Block_rt.top, *resourcedc, 0, 0, 48, 16, RGB(0, 0, 255));
	}

	SelectObject(*resourcedc, Object_bitmap);
	for (int i = 0; i < map.object_count; i++)
	{
		switch (map.objects[i].obj_type)
		{
		case Spike:
			TransparentBlt(*mdc, map.objects[i].Obj_rt.left, map.objects[i].Obj_rt.top, map.objects[i].Obj_rt.right - map.objects[i].Obj_rt.left, map.objects[i].Obj_rt.bottom - map.objects[i].Obj_rt.top, *resourcedc, 0, 8, 16, 8, RGB(0, 0, 255));
			break;
		case Flag:
			TransparentBlt(*mdc, map.objects[i].Obj_rt.left, map.objects[i].Obj_rt.top, map.objects[i].Obj_rt.right - map.objects[i].Obj_rt.left, map.objects[i].Obj_rt.bottom - map.objects[i].Obj_rt.top, *resourcedc, 20, 0, 12, 16, RGB(0, 0, 255));
			break;
		default:
			break;
		}
	}
	
	for (int i = 0; i < map.enemy_count; i++)
	{
		if (map.enemys[i].is_alive)
		{
			if (map.enemys[i].direction == LEFT)
			{
				SelectObject(*resourcedc, Enemy_bitmap);
				TransparentBlt(*mdc, map.enemys[i].x - Size, map.enemys[i].y - Size, (Size * 2), (Size * 2), *resourcedc, 0, 0, 16, 16, RGB(0, 0, 255));
			}
			else
			{
				SelectObject(*resourcedc, Enemy_rv_bitmap);
				TransparentBlt(*mdc, map.enemys[i].x - Size, map.enemys[i].y - Size, (Size * 2), (Size * 2), *resourcedc, 0, 0, 16, 16, RGB(0, 0, 255));
			}
		}
	}

	if (map.boss_count != 0)
	{
		for (int i = 0; i < map.boss_count; i++)
		{
			TransparentBlt(*mdc, map.boss.boss_rect.left, map.boss.boss_rect.top, map.boss.boss_rect.right - map.boss.boss_rect.left, map.boss.boss_rect.bottom - map.boss.boss_rect.top, *resourcedc, 0, 0, 16, 16, RGB(0, 0, 255));
		}
	}
}