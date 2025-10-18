#include <Windows.h>
#include <tchar.h>
#include <time.h>
#include <math.h>

#include "Block&Object.h"
#include "Player&Enemy.h"
#include "resource.h"

#include "fmod.hpp"
#include "fmod_errors.h"

FMOD::System* ssystem;
FMOD::Sound* sound1, * sound2, *sound3;
FMOD::Channel* channel = 0;
FMOD_RESULT result;
void* extradriverdata = 0;

HINSTANCE g_hinst;
LPCTSTR lpszClass = L"Window Class Name";
LPCTSTR lpszWindowName = L"����";

enum DrawMod {
	D_Block, D_Spike, D_Flag, D_Enemy, D_Boss, D_player
};

Map Editmap;

RECT wnd_rt, window_rect, Desk_rect;
Map map;
int selected_map;

Player player;

bool window_move;

bool Clear;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime);


LRESULT CALLBACK WndProcGame(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
void CreateGameWindow(HINSTANCE hInstance);
void CloseGameWindow(HWND hGameWnd);


LRESULT CALLBACK WndEditProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
void CreateEditWindow(HINSTANCE hInstance);
void CloseEditWindow(HWND hGameWnd);

void CreateButtons(HWND hWnd, HBITMAP Start_bitmap, HBITMAP Exit_bitmap, HBITMAP Edit_bitmap) {
	HWND hStartButton = CreateWindow(
		L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_BITMAP,
		50, 450, 200, 50, hWnd, (HMENU)1, g_hinst, NULL);
	SendMessage(hStartButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)Start_bitmap);

	HWND hExitButton = CreateWindow(
		L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_BITMAP,
		550, 450, 200, 50, hWnd, (HMENU)2, g_hinst, NULL);
	SendMessage(hExitButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)Exit_bitmap);

	HWND hEditButton = CreateWindow(
		L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_BITMAP,
		300, 450, 200, 50, hWnd, (HMENU)3, g_hinst, NULL);
	SendMessage(hEditButton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)Edit_bitmap);
}

HBITMAP LoadScaledBitmap(HINSTANCE hInst, int nIDResource, int width, int height) {
	HBITMAP hBitmap = (HBITMAP)LoadImage(hInst, MAKEINTRESOURCE(nIDResource), IMAGE_BITMAP, width, height, LR_CREATEDIBSECTION);
	return hBitmap;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hinst = hInstance;
	Clear = false;
	selected_map = 0;
	GetWindowRect(GetDesktopWindow(), &Desk_rect);

    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"WindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex)) {
        MessageBox(nullptr, L"������ ��� ����", L"����", MB_OK);
        return 1;
    }
	
	HWND hWnd = CreateWindow(L"WindowClass", L"Ÿ��Ʋ ȭ��", WS_OVERLAPPEDWINDOW, (Desk_rect.right / 2) - 400, (Desk_rect.bottom / 2) - 300, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) {
        MessageBox(nullptr, L"������ ���� ����", L"����", MB_OK);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	static HBITMAP Title_bitmap, Clear_bitmap, Start_bitmap, Exit_bitmap, Edit_bitmap;
	static RECT Client_rect;
    switch (message) {
	case WM_CREATE:
		result = FMOD::System_Create(&ssystem); //--- ���� �ý��� ����
		if (result != FMOD_OK)
			exit(0);
		ssystem->init(32, FMOD_INIT_NORMAL, extradriverdata); //--- ���� �ý��� �ʱ�ȭ
		ssystem->createSound("bgm.wav", FMOD_LOOP_NORMAL, 0, &sound1); //--- 1�� ���� ���� �� ����
		ssystem->createSound("button.wav", FMOD_LOOP_OFF, 0, &sound2); //--- 2�� ���� ���� �� ����
		ssystem->createSound("jump.wav", FMOD_LOOP_OFF, 0, &sound3); //--- 3�� ���� ���� �� ����

		channel->stop();
		ssystem->playSound(sound1, 0, false, &channel);

		Title_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP6));
		Clear_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP13));
		Editmap.block_count = Editmap.object_count = Editmap.enemy_count = Editmap.boss_count = 0;
		// ��ư ũ�⿡ �°� ��Ʈ���� �ε��մϴ�.
		Start_bitmap = LoadScaledBitmap(g_hinst, IDB_BITMAP7, 200, 50);
		Exit_bitmap = LoadScaledBitmap(g_hinst, IDB_BITMAP8, 200, 50);
		Edit_bitmap = LoadScaledBitmap(g_hinst, IDB_BITMAP12, 200, 50);
		CreateButtons(hWnd, Start_bitmap, Exit_bitmap, Edit_bitmap);
		GetClientRect(hWnd, &Client_rect);
		break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		switch (wmId) {
		case 1: // ���� ���� ��ư
			channel->stop();
			ssystem->playSound(sound2, 0, false, &channel);
			Sleep(1000);
			// ���� �����츦 �����
			ShowWindow(hWnd, SW_HIDE);
			map = init_map(Desk_rect, &player, selected_map);
			// ���ο� ���� �����츦 ����
			CreateGameWindow(g_hinst);
			break;
		case 2: // ���� ���� ��ư
			channel->stop();
			ssystem->playSound(sound2, 0, false, &channel);
			Sleep(1000);
			DeleteObject(Title_bitmap);
			DeleteObject(Clear_bitmap);
			DeleteObject(Start_bitmap);
			DeleteObject(Exit_bitmap);
			DeleteObject(Edit_bitmap);
			ssystem->release();
			PostQuitMessage(0);
			break;
		case 3:
			channel->stop();
			ssystem->playSound(sound2, 0, false, &channel);
			Sleep(1000);
			ShowWindow(hWnd, SW_HIDE);
			// ���ο� ���� �����츦 ����
			CreateEditWindow(g_hinst);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
    break;
	case WM_SIZE:
		GetClientRect(hWnd, &Client_rect);
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	
    case WM_PAINT :
        {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		HDC hMemDC = CreateCompatibleDC(hdc);
		HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, Title_bitmap);
		if (Clear)
		{
			hOldBitmap = (HBITMAP)SelectObject(hMemDC, Clear_bitmap);
		}
		BITMAP bitmap;
		GetObject(Title_bitmap, sizeof(BITMAP), &bitmap);
		StretchBlt(hdc, 0, 0, Client_rect.right, Client_rect.bottom, hMemDC, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);

		SelectObject(hMemDC, hOldBitmap);

		DeleteDC(hMemDC);
		EndPaint(hWnd, &ps);
		break;
        }
        break;
    case WM_DESTROY:
		DeleteObject(Title_bitmap);
		DeleteObject(Clear_bitmap);
		DeleteObject(Start_bitmap);
		DeleteObject(Exit_bitmap);
		DeleteObject(Edit_bitmap);
		ssystem->release();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CloseGameWindow(HWND hGameWnd) {
	// ���� �����츦 �ݰ� Ÿ��Ʋ �����츦 �ٽ� ������
	DestroyWindow(hGameWnd);
	HWND hTitleWnd = FindWindow(L"WindowClass", NULL);
	if (hTitleWnd) {
		ShowWindow(hTitleWnd, SW_SHOW);
	}
}

void CreateGameWindow(HINSTANCE hInstance) {
	HWND hWnd;
	MSG Message;
	WNDCLASSEX WndClass;
	g_hinst = hInstance;

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = (WNDPROC)WndProcGame;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_ASTERISK);
	WndClass.hCursor = LoadCursor(NULL, IDC_NO);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = L"GameWindow";
	WndClass.hIconSm = LoadIcon(NULL, IDI_ASTERISK);
	RegisterClassEx(&WndClass);

	hWnd = CreateWindow(L"GameWindow", lpszWindowName, WS_CAPTION, 100, 50, 800, 800, NULL, NULL, hInstance, NULL);
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	while (GetMessage(&Message, 0, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
}



void CloseEditWindow(HWND hEditWnd) {
	// ���� �����츦 �ݰ� Ÿ��Ʋ �����츦 �ٽ� ������
	DestroyWindow(hEditWnd);
	HWND hTitleWnd = FindWindow(L"WindowClass", NULL);
	if (hTitleWnd) {
		ShowWindow(hTitleWnd, SW_SHOW);
	}
}

int width = GetSystemMetrics(SM_CXSCREEN);
int height = GetSystemMetrics(SM_CYSCREEN);

void CreateEditWindow(HINSTANCE hInstance) {
	HWND hWnd;
	MSG Message;
	WNDCLASSEX WndClass;
	g_hinst = hInstance;

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = (WNDPROC)WndEditProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_ASTERISK);
	WndClass.hCursor = LoadCursor(NULL, IDC_NO);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = L"EditWindow";
	WndClass.hIconSm = LoadIcon(NULL, IDI_ASTERISK);
	RegisterClassEx(&WndClass);

	hWnd = CreateWindow(L"EditWindow", lpszWindowName, WS_CAPTION, 0, 0, width, height, NULL, NULL, hInstance, NULL);
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	while (GetMessage(&Message, 0, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
}


LRESULT CALLBACK WndProcGame(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
	HDC hdc, mdc, resourcedc;
	HBITMAP hBitmap;
	static HBITMAP Character_bitmap, Enemy_bitmap, Object_bitmap, Platforms_bitmap, BGM_bitmap, BGN_bitmap,Tino_bitmap;
	PAINTSTRUCT ps;
	static RECT Desk_rect;

	static int x, y;

	switch (iMsg) {
	case WM_CREATE:
		result = FMOD::System_Create(&ssystem); //--- ���� �ý��� ����
		if (result != FMOD_OK)
			exit(0);
		ssystem->init(32, FMOD_INIT_NORMAL, extradriverdata); //--- ���� �ý��� �ʱ�ȭ
		ssystem->createSound("bgm.wav", FMOD_LOOP_NORMAL, 0, &sound1); //--- 1�� ���� ���� �� ����
		ssystem->createSound("jump.wav", FMOD_LOOP_OFF, 0, &sound2); //--- 2�� ���� ���� �� ����
		ssystem->createSound("down.wav", FMOD_LOOP_OFF, 0, &sound3); //--- 3�� ���� ���� �� ����
		
	
		channel->stop();
		ssystem->playSound(sound1, 0, false, &channel);
		srand(time(NULL));
		Character_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP4));
		Enemy_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP1));
		Object_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP2));
		Platforms_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP3));
		BGM_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP10));
		BGN_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP11));
		Tino_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP9));
		GetWindowRect(GetDesktopWindow(), &Desk_rect);
		window_move = true;

		SetTimer(hWnd, 1, 1, (TIMERPROC)TimerProc);
		break;
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_CONTROL:
			window_move = !window_move;
			break;
		case VK_LEFT:
			player.LEFT = true;
			break;
		case VK_RIGHT:
			player.RIGHT = true;
			break;
		case VK_UP:
			if (player.jump_count != 0)
			{
				player.UP = true;
				if (player.UP = true) {
					ssystem->playSound(sound2, 0, false, &channel);
				}
				player.is_in_air = true;
				player.jump_time = 0;
				player.jump_count--;
			}
			break;
		case VK_SHIFT:
			if (player.is_in_air)
			{
				player.DOWN = true;
				if (player.DOWN = true) {
					ssystem->playSound(sound3, 0, false, &channel);
				}
			}
			break;
		case VK_RETURN:
			player.is_in_air = true;
			break;
		default:
			break;
		}
		InvalidateRect(hWnd, NULL, false);
		break;
	case WM_KEYUP:
		switch (wParam) {
		case VK_LEFT:
			player.LEFT = false;
			break;
		case VK_RIGHT:
			player.RIGHT = false;
			break;
		case VK_UP:
			break;
		default:
			break;
		}
		InvalidateRect(hWnd, NULL, false);
		break;
	case WM_CHAR:
		switch (wParam)
		{
		case 'q': case 'Q':
			DeleteObject(Tino_bitmap);
			DeleteObject(Enemy_bitmap);
			DeleteObject(Object_bitmap);
			DeleteObject(Platforms_bitmap);
			DeleteObject(BGM_bitmap);
			DeleteObject(BGN_bitmap);
			ssystem->release();
			CloseGameWindow(hWnd); // Ÿ��Ʋ ȭ������ ���ư�.
			break;
		default:
			break;
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		mdc = CreateCompatibleDC(hdc);
		resourcedc = CreateCompatibleDC(hdc);
		hBitmap = CreateCompatibleBitmap(hdc, Desk_rect.right, Desk_rect.bottom);
		SelectObject(mdc, hBitmap);
		Rectangle(mdc, -1, -1, Desk_rect.right, Desk_rect.bottom);
		GetWindowRect(hWnd, &window_rect);

		if (map.boss_count != 0)
		{
			SelectObject(resourcedc, BGN_bitmap);
			TransparentBlt(mdc, 0, 0, Desk_rect.right, Desk_rect.bottom, resourcedc, 0, 0, 2370, 1190, RGB(0, 0, 255));
		}
		else
		{
			SelectObject(resourcedc, BGM_bitmap);
			TransparentBlt(mdc, 0, 0, Desk_rect.right, Desk_rect.bottom, resourcedc, 0, 0, 2370, 1190, RGB(0, 0, 255));
		}


		//ĳ���� �׸���
		Player_Draw(&mdc, &resourcedc, Tino_bitmap, player);

		//�� �׸���
		Draw_Map(&mdc, &resourcedc, Object_bitmap, Platforms_bitmap, Enemy_bitmap, map);

		BitBlt(hdc, 0,0, wnd_rt.right, wnd_rt.bottom, mdc, window_rect.left, window_rect.top, SRCCOPY);
		DeleteDC(mdc);
		DeleteDC(resourcedc);
		DeleteObject(hBitmap);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		DeleteObject(Character_bitmap);
		DeleteObject(Enemy_bitmap);
		DeleteObject(Object_bitmap);
		DeleteObject(Platforms_bitmap);
		DeleteObject(BGM_bitmap);
		DeleteObject(BGN_bitmap);
		ssystem->release();
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}

void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime) {
	RECT temp_rt;
	RECT Desk_rect;
	int randomnum;

	GetClientRect(hWnd, &wnd_rt);
	GetWindowRect(hWnd, &window_rect);
	GetWindowRect(GetDesktopWindow(), &Desk_rect);
	switch (idEvent)
	{
	case 1://�÷��̾� �̵� �� �浹
		
		Player_Move(&player, window_rect);

		if (!IntersectRect(&temp_rt, &player.player_rt, &Desk_rect))//�� ������ ������
		{
			player.x = map.P_start_x;
			player.y = map.P_start_y;
			player.DOWN = false;
			player.is_in_air = false;
			window_move = true;
			player.player_life--;
		}


		if (player.player_life <= 0) {
			CloseGameWindow(hWnd); // Ÿ��Ʋ ȭ������ ���ư�.
		}

		//�� �浹 ����
		for (int i = 0; i < map.block_count; i++)
		{
			if (player.x <= map.blocks[i].Block_rt.right && player.x >= map.blocks[i].Block_rt.left)//�÷��̾ Ư������ x�ȿ� �ִٸ�
			{
				if (player.y + Size > map.blocks[i].Block_rt.top && player.y + Size < map.blocks[i].y)//����
				{
					player.y = map.blocks[i].Block_rt.top - Size;
					player.jump_count = 2;
					player.is_in_air = false;
					player.DOWN = false;
				}
				else if (player.y - Size > map.blocks[i].y && player.y - Size < map.blocks[i].Block_rt.bottom)//�Ʒ�
				{
					player.y = map.blocks[i].Block_rt.bottom + Size;
					player.is_in_air = true;
					player.DOWN = false;
					player.UP = false;
				}
			}
			else if (player.y <= map.blocks[i].Block_rt.bottom && player.y >= map.blocks[i].Block_rt.top)//�÷��̾ Ư������ y�ȿ� �ִٸ�
			{
				if (player.x - Size < map.blocks[i].Block_rt.right && player.x - Size > map.blocks[i].x)//������
				{
					player.UP = false;
					player.x = map.blocks[i].Block_rt.right + Size;
					player.y += 2;
					player.jump_count = 1;
					player.is_in_air = false;
					player.DOWN = false;
				}
				else if (player.x + Size > map.blocks[i].Block_rt.left && player.x + Size < map.blocks[i].x)//����
				{
					player.UP = false;
					player.x = map.blocks[i].Block_rt.left - Size;
					player.y += 2;
					player.jump_count = 1;
					player.is_in_air = false;
					player.DOWN = false;
				}
			}
			
		}

		//������Ʈ �浹 ����
		for (int i = 0; i < map.object_count; i++)
		{
			if (IntersectRect(&temp_rt,&player.player_rt,&map.objects[i].Obj_rt))
			{
				switch (map.objects[i].obj_type)
				{
				case Spike://���� �浹
					player.x = map.P_start_x;
					player.y = map.P_start_y;
					player.DOWN = false;
					player.is_in_air = false;
					window_move = true;

					player.player_life--; // ��� ����
					break;
				case Flag://�������� �浹
					if (selected_map == 99)
					{
						selected_map = 0;
						CloseGameWindow(hWnd);
						CreateEditWindow(g_hinst);
					}
					map = init_map(Desk_rect, &player, ++selected_map);
					player.x = map.P_start_x;
					player.y = map.P_start_y;
					player.DOWN = false;
					player.is_in_air = false;
					window_move = true;
					break;
				default:
					break;
				}
			}
		}

		//�� �浹 ����
		for (int i = 0; i < map.enemy_count; i++)
		{
			if (map.enemys[i].is_alive)
			{
				if (IntersectRect(&temp_rt, &player.player_rt, &map.enemys[i].enemy_rect)) {
					if (player.DOWN)
					{
						map.enemys[i].is_alive = false;
					}
					else
					{
						player.x = map.P_start_x;
						player.y = map.P_start_y;
						player.DOWN = false;
						player.is_in_air = false;
						window_move = true;

						player.player_life--; // ��� ����
					}
				}
			}
		}
		
		//�� ������
		for (int i = 0; i < map.enemy_count; i++)
		{
			Move_Enemy(&map.enemys[i], map.blocks[map.enemys[i].on_block], 3);
			Update_Enemy_rect(&map.enemys[i]);
		}

		if (window_move)
		{
			MoveWindow(hWnd, player.x - (wnd_rt.right / 2), player.y - (wnd_rt.bottom / 2), 320, 320, true);
		}
		else
		{
			MoveWindow(hWnd, window_rect.left, player.y - (wnd_rt.bottom / 2), 320, 320, true);
		}

		
		//������
		if (map.boss_count != 0)
		{
			if (map.boss.attack_time <= 0)//���� �ֱ⸶�� �� ����
			{
				switch (map.boss.life)
				{
				case 1:
					map.blocks[map.block_count].x = map.boss.x;
					map.blocks[map.block_count].y = Desk_rect.bottom - ((rand() % 640) + 100);
					map.blocks[map.block_count].Block_rt = { map.blocks[map.block_count].x - 40,map.blocks[map.block_count].y - 32,map.blocks[map.block_count].x + 40,map.blocks[map.block_count].y + 32 };
					map.block_count++;

					randomnum = rand() % 2;
					if (randomnum == 0)
					{
						map.enemys[map.enemy_count] = Make_Enemy(map.blocks[map.block_count - 1].x, map.blocks[map.block_count - 1].Block_rt.top - Size, map.block_count - 1);
						map.enemy_count++;
					}

					map.boss.attack_time = 160;
					break;
				case 2:
					map.blocks[map.block_count].x = map.boss.x;
					map.blocks[map.block_count].y = Desk_rect.bottom - ((rand() % 512) + 100);
					map.blocks[map.block_count].Block_rt = { map.blocks[map.block_count].x - 80,map.blocks[map.block_count].y - 32,map.blocks[map.block_count].x + 80,map.blocks[map.block_count].y + 32 };
					map.block_count++;

					randomnum = rand() % 2;
					if (randomnum == 0)
					{
						map.enemys[map.enemy_count] = Make_Enemy(map.blocks[map.block_count - 1].x, map.blocks[map.block_count - 1].Block_rt.top - Size, map.block_count - 1);
						map.enemy_count++;
					}

					map.boss.attack_time = 320;
					break;
				case 3:
					map.blocks[map.block_count].x = map.boss.x;
					map.blocks[map.block_count].y = Desk_rect.bottom - ((rand() % 256) + 100);
					map.blocks[map.block_count].Block_rt = { map.blocks[map.block_count].x - 160,map.blocks[map.block_count].y - 32,map.blocks[map.block_count].x + 160,map.blocks[map.block_count].y + 32 };
					map.block_count++;

					randomnum = rand() % 2;
					if (randomnum == 0)
					{
						map.enemys[map.enemy_count] = Make_Enemy(map.blocks[map.block_count - 1].x, map.blocks[map.block_count - 1].Block_rt.top - Size, map.block_count - 1);
						map.enemy_count++;
					}

					map.boss.attack_time = 480;
					break;
				default:
					break;
				}
			}

			if (IntersectRect(&temp_rt, &player.player_rt, &map.boss.boss_rect)) {
				if (player.DOWN)
				{
					map.boss.life--;
					if (map.boss.life <= 0)
					{
						Clear = true;
						selected_map = 0;
						CloseGameWindow(hWnd);
					}
					player.x = map.P_start_x;
					player.y = map.P_start_y;
					player.DOWN = false;
					player.is_in_air = false;
					window_move = true;
					player.player_life++;
				}
				else
				{
					player.x = map.P_start_x;
					player.y = map.P_start_y;
					player.DOWN = false;
					player.is_in_air = false;
					window_move = true;

					player.player_life--; // ��� ����
				}
			}

			for (int i = 0; i < map.block_count; i++)
			{
				if (player.x <= map.blocks[i].Block_rt.right && player.x >= map.blocks[i].Block_rt.left)//�÷��̾ Ư������ x�ȿ� �ִٸ�
				{
					if (player.y + Size >= map.blocks[i].Block_rt.top && player.y + Size < map.blocks[i].y)//����
					{
						player.x--;
					}
				}

				if (map.blocks[i].x <= Desk_rect.left)//���� �� ������ ������
				{
					for (int j = i; j < map.block_count - 1; j++)
					{
						map.blocks[j] = map.blocks[j + 1];

						for (int k = 0; k < map.enemy_count; k++)
						{
							if (map.enemys[k].on_block == i)
							{
								for (int l = k; k < map.enemy_count; k++) {
									map.enemys[l] = map.enemys[l + 1];
								}
								map.enemy_count--;
							}
							else if (map.enemys[k].on_block == j + 1)
							{
								map.enemys[k].on_block = j;
							}
						}
					}
					map.block_count--;
					
				}
				map.blocks[i].x--;
				map.blocks[i].Block_rt = { map.blocks[i].Block_rt.left - 1, map.blocks[i].Block_rt.top, map.blocks[i].Block_rt.right - 1, map.blocks[i].Block_rt.bottom };
			}

			for (int i = 0; i < map.enemy_count; i++)
			{
				if (map.enemys[i].is_alive)
				{
					map.enemys[i].x--;
					Update_Enemy_rect(&map.enemys[i]);
				}
			}

			map.boss.attack_time--;
			map.P_start_x = map.blocks[0].x;
			map.P_start_y = map.blocks[0].Block_rt.top - Size;
		}
		break;
	default:
		break;
	}
	InvalidateRect(hWnd, NULL, false);
}


double get_length(int x1, int y1, int x2, int y2) {
	return sqrt((((double)x2 - (double)x1) * ((double)x2 - (double)x1)) + (((double)y2 - (double)y1) * ((double)y2 - (double)y1)));
}

int search_near_Block(int x, int y) {
	int nearest = 0;
	for (int i = 0; i < Editmap.block_count; i++)
	{
		if (get_length(x, y, Editmap.blocks[i].x, Editmap.blocks[i].y) <= get_length(x, y, Editmap.blocks[nearest].x, Editmap.blocks[nearest].y))
		{
			nearest = i;
		}
	}
	return nearest;
}

int search_near(int x, int y, DrawMod curDrawmod) {
	int nearest = 0;
	switch (curDrawmod)
	{
	case D_Block:
		for (int i = 0; i < Editmap.block_count; i++)
		{
			if (get_length(x, y, Editmap.blocks[i].x, Editmap.blocks[i].y) <= get_length(x, y, Editmap.blocks[nearest].x, Editmap.blocks[nearest].y))
			{
				nearest = i;
			}
		}
		break;
	case D_Spike:
	case D_Flag:
		for (int i = 0; i < Editmap.object_count; i++)
		{
			if (get_length(x, y, Editmap.objects[i].x, Editmap.objects[i].y) <= get_length(x, y, Editmap.objects[nearest].x, Editmap.objects[nearest].y))
			{
				nearest = i;
			}
		}
		break;
	case D_Enemy:
		for (int i = 0; i < Editmap.enemy_count; i++)
		{
			if (get_length(x, y, Editmap.enemys[i].x, Editmap.enemys[i].y) <= get_length(x, y, Editmap.enemys[nearest].x, Editmap.enemys[nearest].y))
			{
				nearest = i;
			}
		}
		break;
	default:
		break;
	}
	return nearest;
}

//EditWindow �Լ�
LRESULT CALLBACK WndEditProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	HDC hdc, mdc, resourcedc;
	HBITMAP hBitmap;
	static HBITMAP Character_bitmap, Enemy_bitmap, Object_bitmap, Platforms_bitmap, BGM_bitmap, BGN_bitmap, Tino_bitmap;
	PAINTSTRUCT ps;
	static RECT Desk_rect;

	static DrawMod curDrawmod;

	static int start_x, start_y, old_x, old_y;
	int end_x, end_y;
	static bool down;

	switch (iMessage) {
	case WM_CREATE:
		srand(time(NULL));
		Character_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP4));
		Enemy_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP1));
		Object_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP2));
		Platforms_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP3));
		BGM_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP10));
		BGN_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP11));
		Tino_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP9));
		GetWindowRect(GetDesktopWindow(), &Desk_rect);
		down = false;
		curDrawmod = D_Block;
		break;
	case WM_LBUTTONDOWN:
		start_y = old_y = HIWORD(lParam);//������ ���� ��ǥ�� ��ȯ
		start_x = old_x = LOWORD(lParam);
		switch (curDrawmod)
		{
		case D_Block:
			down = true;
			Editmap.blocks[Editmap.block_count].Block_rt = { start_x,start_y,old_x,old_y };
			Editmap.block_count++;
			break;
		case D_Spike:
			end_x = search_near_Block(start_x, start_y);
			Editmap.objects[Editmap.object_count].x = start_x;
			Editmap.objects[Editmap.object_count].y = Editmap.blocks[end_x].Block_rt.top - 8;
			Editmap.objects[Editmap.object_count].obj_type = Spike;
			Editmap.objects[Editmap.object_count].Obj_rt = { (Editmap.objects[Editmap.object_count].x - Size),(Editmap.objects[Editmap.object_count].y - Size),(Editmap.objects[Editmap.object_count].x + Size),(Editmap.objects[Editmap.object_count].y + Size) };
			Editmap.object_count++;
			break;
		case D_Flag:
			end_x = search_near_Block(start_x, start_y);
			Editmap.objects[Editmap.object_count].x = start_x;
			Editmap.objects[Editmap.object_count].y = Editmap.blocks[end_x].Block_rt.top - 8;
			Editmap.objects[Editmap.object_count].obj_type = Flag;
			Editmap.objects[Editmap.object_count].Obj_rt = { (Editmap.objects[Editmap.object_count].x - Size),(Editmap.objects[Editmap.object_count].y - Size),(Editmap.objects[Editmap.object_count].x + Size),(Editmap.objects[Editmap.object_count].y + Size) };
			Editmap.object_count++;
			break;
		case D_Enemy:
			end_x = search_near_Block(start_x, start_y);
			Editmap.enemys[Editmap.enemy_count] = Make_Enemy(start_x, Editmap.blocks[end_x].Block_rt.top - Size, end_x);
			Editmap.enemy_count++;
			break;
		case D_Boss:
			Editmap.boss.x = start_x;
			Editmap.boss.y = start_y;
			Editmap.boss.life = 3;
			Editmap.boss.boss_rect = { Editmap.boss.x - 160,Editmap.boss.y - 160,Editmap.boss.x + 160,Editmap.boss.y + 160 };
			Editmap.boss_count++;
			break;
		case D_player:
			Editmap.P_start_x = start_x;
			Editmap.P_start_y = start_y;
			player = Make_Player(Editmap.P_start_x, Editmap.P_start_y);
			break;
		default:
			break;
		}
		break;
	case WM_MOUSEMOVE:
		if (down)
		{
			end_x = LOWORD(lParam);
			end_y = HIWORD(lParam);
			Editmap.blocks[Editmap.block_count - 1].Block_rt = { start_x,start_y,end_x,end_y };

		}
		InvalidateRect(hWnd, NULL, false);
		break;
	case WM_LBUTTONUP:
		switch (curDrawmod)
		{
		case D_Block:
			end_x = LOWORD(lParam);
			end_y = HIWORD(lParam);
			Editmap.blocks[Editmap.block_count - 1].Block_rt = { start_x,start_y,end_x,end_y };
			Editmap.blocks[Editmap.block_count - 1].x = Editmap.blocks[Editmap.block_count - 1].Block_rt.left + ((Editmap.blocks[Editmap.block_count - 1].Block_rt.right - Editmap.blocks[Editmap.block_count - 1].Block_rt.left) / 2);
			Editmap.blocks[Editmap.block_count - 1].y = Editmap.blocks[Editmap.block_count - 1].Block_rt.top + ((Editmap.blocks[Editmap.block_count - 1].Block_rt.bottom - Editmap.blocks[Editmap.block_count - 1].Block_rt.top) / 2);
			down = false;
			break;
		default:
			break;
		}
		break;
	case WM_RBUTTONDOWN:
		start_y = old_y = HIWORD(lParam);
		start_x = old_x = LOWORD(lParam);
		switch (curDrawmod)
		{
		case D_Block:
			if (Editmap.block_count == 0)
				break;
			end_x = search_near(start_x, start_y, curDrawmod);
			for (int i = end_x; i < Editmap.block_count; i++)
			{
				Editmap.blocks[i] = Editmap.blocks[i + 1];
			}
			Editmap.block_count--;
			break;
		case D_Spike:
		case D_Flag:
			if (Editmap.object_count == 0)
				break;
			end_x = search_near(start_x, start_y, curDrawmod);
			for (int i = end_x; i < Editmap.object_count; i++)
			{
				Editmap.objects[i] = Editmap.objects[i + 1];
			}
			Editmap.object_count--;
			break;
		case D_Enemy:
			if (Editmap.enemy_count == 0)
				break;
			end_x = search_near(start_x, start_y, curDrawmod);
			for (int i = end_x; i < Editmap.enemy_count; i++)
			{
				Editmap.enemys[i] = Editmap.enemys[i + 1];
			}
			Editmap.enemy_count--;
			break;
		case D_Boss:
			Editmap.boss_count = 0;
			break;
		case D_player:
			Editmap.P_start_x = 0;
			Editmap.P_start_y = 0;
			player = Make_Player(Editmap.P_start_x, Editmap.P_start_y);
			break;
		default:
			break;
		}
		break;
	case WM_CHAR:
		switch (wParam)
		{
		case 's': case 'S': //�׽�Ʈ
			map = Editmap;
			selected_map = 99;
			CloseEditWindow(hWnd);
			CreateGameWindow(g_hinst);
			break;
		case '1':
			curDrawmod = D_Block;
			break;
		case '2':
			curDrawmod = D_Spike;
			break;
		case '3':
			curDrawmod = D_Flag;
			break;
		case '4':
			curDrawmod = D_Enemy;
			break;
		case '5':
			curDrawmod = D_Boss;
			break;
		case '6':
			curDrawmod = D_player;
			break;
		case 'q': case 'Q':
			DeleteObject(Character_bitmap);
			DeleteObject(Enemy_bitmap);
			DeleteObject(Object_bitmap);
			DeleteObject(Platforms_bitmap);
			DeleteObject(BGM_bitmap);
			DeleteObject(BGN_bitmap);
			CloseEditWindow(hWnd); // Ÿ��Ʋ ȭ������ ���ư�.
			break;
		default:
			break;
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		mdc = CreateCompatibleDC(hdc);
		resourcedc = CreateCompatibleDC(hdc);
		hBitmap = CreateCompatibleBitmap(hdc, Desk_rect.right, Desk_rect.bottom);
		SelectObject(mdc, hBitmap);
		Rectangle(mdc, -1, -1, Desk_rect.right, Desk_rect.bottom);
		GetWindowRect(hWnd, &window_rect);
		GetClientRect(hWnd, &wnd_rt);

		if (map.boss_count != 0)
		{
			SelectObject(resourcedc, BGN_bitmap);
			TransparentBlt(mdc, 0, 0, Desk_rect.right, Desk_rect.bottom, resourcedc, 0, 0, 2370, 1190, RGB(0, 0, 255));
		}
		else
		{
			SelectObject(resourcedc, BGM_bitmap);
			TransparentBlt(mdc, 0, 0, Desk_rect.right, Desk_rect.bottom, resourcedc, 0, 0, 2370, 1190, RGB(0, 0, 255));
		}

		//ĳ���� �׸���
		SelectObject(resourcedc, Tino_bitmap);
		TransparentBlt(mdc, Editmap.P_start_x - Size, Editmap.P_start_y - Size, (Size * 2), (Size * 2), resourcedc, 0, 0, 150, 140, RGB(0, 0, 255));

		//�� �׸���
		Draw_Map(&mdc, &resourcedc, Object_bitmap, Platforms_bitmap, Enemy_bitmap, Editmap);

		//���� �׸��� ��� �׸���
		switch (curDrawmod)
		{
		case D_Block:
			SelectObject(resourcedc, Platforms_bitmap);
			TransparentBlt(mdc, Desk_rect.right - 64, Desk_rect.top, 64, 64, resourcedc, 0, 0, 48, 16, RGB(0, 0, 255));
			break;
		case D_Spike:
			SelectObject(resourcedc, Object_bitmap);
			TransparentBlt(mdc, Desk_rect.right - 64, Desk_rect.top, 64, 64, resourcedc, 0, 8, 16, 8, RGB(0, 0, 255));
			break;
		case D_Flag:
			SelectObject(resourcedc, Object_bitmap);
			TransparentBlt(mdc, Desk_rect.right - 64, Desk_rect.top, 64, 64, resourcedc, 20, 0, 12, 16, RGB(0, 0, 255));

			break;
		case D_Enemy:
			SelectObject(resourcedc, Enemy_bitmap);
			TransparentBlt(mdc, Desk_rect.right - 64, Desk_rect.top, 64, 64, resourcedc, 0, 0, 16, 16, RGB(0, 0, 255));
			break;
		case D_Boss:
			SelectObject(resourcedc, Enemy_bitmap);
			TransparentBlt(mdc, Desk_rect.right - 128, Desk_rect.top, 128, 128, resourcedc, 0, 0, 16, 16, RGB(0, 0, 255));
			break;
		case D_player:
			SelectObject(resourcedc, Tino_bitmap);
			TransparentBlt(mdc, Desk_rect.right - 128, Desk_rect.top, 128, 128, resourcedc, 0, 0, 150, 140, RGB(0, 0, 255));
			break;
		default:
			break;
		}

		BitBlt(hdc, 0, 0, wnd_rt.right, wnd_rt.bottom, mdc, 0, 0, SRCCOPY);
		DeleteDC(mdc);
		DeleteDC(resourcedc);
		DeleteObject(hBitmap);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		DeleteObject(Character_bitmap);
		DeleteObject(Enemy_bitmap);
		DeleteObject(Object_bitmap);
		DeleteObject(Platforms_bitmap);
		DeleteObject(BGM_bitmap);
		DeleteObject(BGN_bitmap);
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, iMessage, wParam, lParam);
}