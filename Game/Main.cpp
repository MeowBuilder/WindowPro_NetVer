#include "Packets.h"
#include <Windows.h>
#include <tchar.h>
#include <time.h>
#include <math.h>

#include "Block&Object.h"
#include "Player&Enemy.h"
#include "resource.h"

#include "ClientSystem.h"
#include "Packets.h"

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

ClientSystem client;

HBITMAP g_hFullMapBitmap = NULL; // 전체 맵을 그릴 공유 비트맵
RECT g_MapRect = { 0, 0, 1920, 1080 }; // 맵 전체 크기 (Desk_rect와 동일하게 관리)

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

int width = 1920;
int height = 1080;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
void CALLBACK StartTimer(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime);


LRESULT CALLBACK WndProcGame(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime);
void CreateGameWindow(HINSTANCE hInstance);
void CreateGameWindow2(HINSTANCE hInstance);
void CloseGameWindow(HWND hGameWnd);

LRESULT CALLBACK WndProcGame2(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
void CALLBACK TimerProc2(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime);


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
	Desk_rect = { 0,0,1920,1080 };

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

	HWND hWnd = CreateWindow(L"WindowClass", L"WindowPro", WS_OVERLAPPEDWINDOW, (Desk_rect.right / 2) - 400, (Desk_rect.bottom / 2) - 300, 800, 600, nullptr, nullptr, hInstance, nullptr);

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
	CS_StartSessionRequestPacket SSRP(0);
	static HBITMAP Title_bitmap, Clear_bitmap, Start_bitmap, Exit_bitmap, Edit_bitmap;
	static RECT Client_rect;
    switch (message) {
	case WM_CREATE:
		result = FMOD::System_Create(&ssystem);
		if (result != FMOD_OK)
			exit(0);
		ssystem->init(32, FMOD_INIT_NORMAL, extradriverdata);
		ssystem->createSound("bgm.wav", FMOD_LOOP_NORMAL, 0, &sound1);
		ssystem->createSound("button.wav", FMOD_LOOP_OFF, 0, &sound2);
		ssystem->createSound("jump.wav", FMOD_LOOP_OFF, 0, &sound3);

		channel->stop();
		ssystem->playSound(sound1, 0, false, &channel);

		Title_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP6));
		Clear_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP13));
		Editmap.block_count = Editmap.object_count = Editmap.enemy_count = Editmap.boss_count = 0;

		Start_bitmap = LoadScaledBitmap(g_hinst, IDB_BITMAP7, 200, 50);
		Exit_bitmap = LoadScaledBitmap(g_hinst, IDB_BITMAP8, 200, 50);
		Edit_bitmap = LoadScaledBitmap(g_hinst, IDB_BITMAP12, 200, 50);
		CreateButtons(hWnd, Start_bitmap, Exit_bitmap, Edit_bitmap);
		GetClientRect(hWnd, &Client_rect);
		client.my_player_id = 0;

		client.Connect("127.0.0.1", 9000);
		client.StartRecvThread();

		SetTimer(hWnd, 0, 0.016, (TIMERPROC)StartTimer);
		break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		switch (wmId) {
		case 1:
			if (client.my_player_id != 0) break;
			channel->stop();
			ssystem->playSound(sound2, 0, false, &channel);
			Sleep(1000);

			ShowWindow(hWnd, SW_HIDE);

			client.SendStartSessionRequestPacket(&SSRP);
			/*
			selected_map = 0;
			map = client.GetMap();
			player = Make_Player(map.P_Start_Loc[0].x, map.P_Start_Loc[0].y);

			CreateGameWindow(g_hinst);
			CreateGameWindow2(g_hinst);*/
			//CreateGameWindow3(g_hinst);
			break;
		case 2:
			channel->stop();
			ssystem->playSound(sound2, 0, false, &channel);
			Sleep(1000);
			DeleteObject(Title_bitmap);
			DeleteObject(Clear_bitmap);
			DeleteObject(Start_bitmap);
			DeleteObject(Exit_bitmap);
			DeleteObject(Edit_bitmap);
			ssystem->release();
			client.Disconnect();
			PostQuitMessage(0);
			break;
		case 3:
			if (client.my_player_id != 0) break;
			channel->stop();
			ssystem->playSound(sound2, 0, false, &channel);
			Sleep(1000);
			ShowWindow(hWnd, SW_HIDE);

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
		client.Disconnect();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


void CALLBACK StartTimer(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime) {
	if (client.StartGame)
	{
		ShowWindow(hWnd, SW_HIDE);

		map = client.GetMap();
		player = Make_Player(map.P_Start_Loc[client.my_player_id].x, map.P_Start_Loc[client.my_player_id].y);

		CreateGameWindow(g_hinst);
		CreateGameWindow2(g_hinst);
		client.StartGame = false;
	}
}

void CloseGameWindow(HWND hGameWnd) {
	HWND hTitleWnd = FindWindow(L"WindowClass", NULL);

	// 1. 메인 게임 윈도우 찾기 및 닫기
	HWND hMainGameWnd = FindWindow(L"GameWindow", NULL);
	if (hMainGameWnd) {
		DestroyWindow(hMainGameWnd);
	}

	// 2. 다른 플레이어 윈도우 찾기 및 닫기
	HWND hOtherPlayerWnd = FindWindow(L"GameWindow2", NULL);
	if (hOtherPlayerWnd) {
		DestroyWindow(hOtherPlayerWnd);
	}

	//UnregisterClass(L"GameWindow", g_hinst);
	//UnregisterClass(L"GameWindow2", g_hinst);

	// 타이틀 윈도우 다시 표시
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
}

void CreateGameWindow2(HINSTANCE hInstance) {
	HWND hWnd;
	WNDCLASSEX WndClass;
	g_hinst = hInstance;

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = (WNDPROC)WndProcGame2; // ★ WndProcGame2를 사용
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_ASTERISK);
	WndClass.hCursor = LoadCursor(NULL, IDC_NO);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = L"GameWindow2"; // 고유한 클래스 이름
	WndClass.hIconSm = LoadIcon(NULL, IDI_ASTERISK);
	RegisterClassEx(&WndClass);

	// 메인 게임 윈도우와 겹치지 않도록 위치와 크기를 조정할 수 있습니다.
	hWnd = CreateWindow(L"GameWindow2", L"Other Player", WS_CAPTION, 950, 50, 320, 320, NULL, NULL, hInstance, NULL);
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);
}

void CloseEditWindow(HWND hEditWnd) {
	DestroyWindow(hEditWnd);
	HWND hTitleWnd = FindWindow(L"WindowClass", NULL);
	if (hTitleWnd) {
		ShowWindow(hTitleWnd, SW_SHOW);
	}
}

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

void CALLBACK TimerProc2(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime) {
	RECT temp_rt;
	RECT Desk_rect;
	int randomnum;

	GetClientRect(hWnd, &wnd_rt);
	GetWindowRect(hWnd, &window_rect);
	Player* hPlayer;
	hPlayer = client.getPlayer(0);
	for (int i = 0; i < 2; i++)
	{
		if (i = client.my_player_id)
		{
			continue;
		}

		hPlayer = client.getPlayer(i);
		break;
	}
	
	Desk_rect = { 0,0,1920,1080 };
	switch (idEvent)
	{
	case 1:
		//hPlayer->x++; //테스트용 실제 충돌처리는 서버에서 진행

		if (window_move)
		{
			MoveWindow(hWnd, std::clamp(hPlayer->x - (wnd_rt.right / 2), (long)0, (long)Desk_rect.right - (wnd_rt.right)), hPlayer->y - (wnd_rt.bottom / 2), 320, 320, true);
		}
		else
		{
			MoveWindow(hWnd, window_rect.left, hPlayer->y - (wnd_rt.bottom / 2), 320, 320, true);
		}
	default:
		break;
	}
	InvalidateRect(hWnd, NULL, false);
}

LRESULT CALLBACK WndProcGame2(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
	HDC hdc, mdc;
	PAINTSTRUCT ps;

	static int x, y;
	switch (iMsg) {
	case WM_CREATE:
		//result = FMOD::System_Create(&ssystem); //--- ���� �ý��� ����
		//if (result != FMOD_OK)
		//	exit(0);
		//ssystem->init(32, FMOD_INIT_NORMAL, extradriverdata); //--- ���� �ý��� �ʱ�ȭ
		//ssystem->createSound("bgm.wav", FMOD_LOOP_NORMAL, 0, &sound1); //--- 1�� ���� ���� �� ����
		//ssystem->createSound("jump.wav", FMOD_LOOP_OFF, 0, &sound2); //--- 2�� ���� ���� �� ����
		//ssystem->createSound("down.wav", FMOD_LOOP_OFF, 0, &sound3);
		//channel->stop();
		//ssystem->playSound(sound1, 0, false, &channel);

		srand(time(NULL));

		SetTimer(hWnd, 1, 0.016, (TIMERPROC)TimerProc2);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);

		// 1. 이미 그려진 전역 비트맵이 있는지 확인
		if (g_hFullMapBitmap) {
			mdc = CreateCompatibleDC(hdc);

			// 2. 전역 비트맵을 선택 (읽기 모드)
			HBITMAP hOldBitmap = (HBITMAP)SelectObject(mdc, g_hFullMapBitmap);

			// 3. 현재 이 윈도우의 위치 가져오기
			GetWindowRect(hWnd, &window_rect);
			GetClientRect(hWnd, &wnd_rt);

			// 4. 전역 비트맵에서 내 윈도우 위치에 해당하는 부분만 가져와서 그리기
			// window_rect.left, top은 전체 화면(맵) 기준의 좌표라고 가정
			BitBlt(hdc, 0, 0, wnd_rt.right, wnd_rt.bottom,
				mdc, window_rect.left, window_rect.top, SRCCOPY);

			// 정리
			SelectObject(mdc, hOldBitmap);
			DeleteDC(mdc);
		}
		else {
			// 아직 메인 윈도우가 비트맵을 안 만들었으면 흰색 등으로 채움
			Rectangle(hdc, 0, 0, 320, 320);
		}

		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		//ssystem->release();
		break;
	}
	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}


LRESULT CALLBACK WndProcGame(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
	HDC hdc, mdc, resourcedc;
	HBITMAP hOldBitmap;
	static HBITMAP Character_bitmap, Enemy_bitmap, Enemy_rv_bitmap, Object_bitmap, Platforms_bitmap, BGM_bitmap, BGN_bitmap,Tino_bitmap;
	PAINTSTRUCT ps;

	static int x, y;
	int id = 0;
	switch (iMsg) {
	case WM_CREATE:
		result = FMOD::System_Create(&ssystem);
		if (result != FMOD_OK) exit(0);
		ssystem->init(32, FMOD_INIT_NORMAL, extradriverdata); 
		ssystem->createSound("bgm.wav", FMOD_LOOP_NORMAL, 0, &sound1);
		ssystem->createSound("jump.wav", FMOD_LOOP_OFF, 0, &sound2);
		ssystem->createSound("down.wav", FMOD_LOOP_OFF, 0, &sound3); 
		
	
		channel->stop();
		ssystem->playSound(sound1, 0, false, &channel);
		srand(time(NULL));
		Character_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP4));
		Enemy_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP1));
		Enemy_rv_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP14));
		Object_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP2));
		Platforms_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP3));
		BGM_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP10));
		BGN_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP11));
		Tino_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP9));

		window_move = true;
		player.player_life = 3;

		// Desk_rect 초기화
		Desk_rect = { 0,0,1920,1080 };

		// [수정] 전역 비트맵 생성 (한 번만 생성하거나 크기가 바뀔 때 재생성)
		hdc = GetDC(hWnd);
		g_hFullMapBitmap = CreateCompatibleBitmap(hdc, Desk_rect.right, Desk_rect.bottom);
		ReleaseDC(hWnd, hdc);

		SetTimer(hWnd, 1, 0.016, (TIMERPROC)TimerProc);
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
			CloseGameWindow(hWnd);
			break;
		default:
			break;
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		mdc = CreateCompatibleDC(hdc);
		resourcedc = CreateCompatibleDC(hdc);

		hOldBitmap = (HBITMAP)SelectObject(mdc, g_hFullMapBitmap);

		// 1. 배경 초기화
		Rectangle(mdc, -1, -1, Desk_rect.right + 1, Desk_rect.bottom + 1);
		GetWindowRect(hWnd, &window_rect); // 현재 윈도우 위치

		// 2. 배경 이미지 그리기
		if (map.boss_count != 0) {
			SelectObject(resourcedc, BGN_bitmap);
			TransparentBlt(mdc, 0, 0, Desk_rect.right, Desk_rect.bottom, resourcedc, 0, 0, 2370, 1190, RGB(0, 0, 255));
		}
		else {
			SelectObject(resourcedc, BGM_bitmap);
			TransparentBlt(mdc, 0, 0, Desk_rect.right, Desk_rect.bottom, resourcedc, 0, 0, 2370, 1190, RGB(0, 0, 255));
		}

		// 3. 플레이어들 그리기 (내 캐릭터 + 다른 플레이어들)
		// Player_Draw 함수가 mdc에 그리도록 되어 있으므로 순서대로 호출
		Player_Draw(&mdc, &resourcedc, Tino_bitmap, player);
		for (id; id < 2; id++)
		{
			if (id == client.my_player_id)
			{
				continue;
			}
			Player_Draw(&mdc, &resourcedc, Tino_bitmap, *client.getPlayer(id));
		}
		//Player_Draw(&mdc, &resourcedc, Tino_bitmap, player);           // 나
		//Player_Draw(&mdc, &resourcedc, Tino_bitmap, *client.getPlayer(1)); // 플레이어 2
		//Player_Draw(&mdc, &resourcedc, Tino_bitmap, client.getPlayer(2)); // 플레이어 3 (필요시)

		// 4. 맵 오브젝트 그리기
		Draw_Map(&mdc, &resourcedc, Object_bitmap, Platforms_bitmap, Enemy_bitmap, Enemy_rv_bitmap, map);

		// 5. [출력] mdc(전역 비트맵)의 내용을 실제 화면(hdc)으로 복사
		// 윈도우가 맵의 일부만 보여주는 카메라 역할을 한다면 window_rect 좌표를 사용
		BitBlt(hdc, 0, 0, wnd_rt.right, wnd_rt.bottom, mdc, window_rect.left, window_rect.top, SRCCOPY);

		// 정리
		SelectObject(mdc, hOldBitmap); // 비트맵 해제 (그래야 다른 DC에서 선택 가능)
		DeleteDC(mdc);
		DeleteDC(resourcedc);

		// 주의: g_hFullMapBitmap은 여기서 지우지 않습니다! (전역 변수이므로)
		EndPaint(hWnd, &ps);

		break;
	case WM_DESTROY:
		DeleteObject(Character_bitmap);
		DeleteObject(Enemy_bitmap);
		DeleteObject(Object_bitmap);
		DeleteObject(Platforms_bitmap);
		DeleteObject(BGM_bitmap);
		DeleteObject(BGN_bitmap);
		if (g_hFullMapBitmap) DeleteObject(g_hFullMapBitmap);
		ssystem->release();
		break;
	}
	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}

void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT idEvent, DWORD dwTime) {
	RECT temp_rt;
	int randomnum;

	GetClientRect(hWnd, &wnd_rt);
	GetWindowRect(hWnd, &window_rect);
	switch (idEvent)
	{
	case 1:
		Player_Move(&player, window_rect);

		if (!IntersectRect(&temp_rt, &player.player_rt, &Desk_rect))
		{
			player.x = map.P_Start_Loc[client.my_player_id].x;
			player.y = map.P_Start_Loc[client.my_player_id].y;
			player.DOWN = false;
			player.is_in_air = false;
			window_move = true;
			player.player_life--;
		}


		if (player.player_life <= 0) {
			CloseGameWindow(hWnd);
		}

		for (int i = 0; i < map.block_count; i++)
		{
			if (player.x <= map.blocks[i].Block_rt.right && player.x >= map.blocks[i].Block_rt.left)
			{
				if (player.y + Size > map.blocks[i].Block_rt.top && player.y + Size < map.blocks[i].y)
				{
					player.y = map.blocks[i].Block_rt.top - Size;
					player.jump_count = 2;
					player.is_in_air = false;
					player.DOWN = false;
				}
				else if (player.y - Size > map.blocks[i].y && player.y - Size < map.blocks[i].Block_rt.bottom)
				{
					player.y = map.blocks[i].Block_rt.bottom + Size;
					player.is_in_air = true;
					player.DOWN = false;
					player.UP = false;
				}
			}
			else if (player.y <= map.blocks[i].Block_rt.bottom && player.y >= map.blocks[i].Block_rt.top)
			{
				if (player.x - Size < map.blocks[i].Block_rt.right && player.x - Size > map.blocks[i].x)
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

		for (int i = 0; i < map.object_count; i++)
		{
			if (IntersectRect(&temp_rt,&player.player_rt,&map.objects[i].Obj_rt))
			{
				switch (map.objects[i].obj_type)
				{
				case Spike:
					player.x = map.P_Start_Loc[0].x;
					player.y = map.P_Start_Loc[0].y;
					player.DOWN = false;
					player.is_in_air = false;
					window_move = true;

					player.player_life--;
					break;
				case Flag:
					if (selected_map == 99)
					{
						selected_map = 0;
						CloseGameWindow(hWnd);
						CreateEditWindow(g_hinst);
					}
					map = init_map(Desk_rect, &player, ++selected_map);
					player.x = map.P_Start_Loc[0].x;
					player.y = map.P_Start_Loc[0].y;
					player.DOWN = false;
					player.is_in_air = false;
					window_move = true;
					break;
				default:
					break;
				}
				break;
			}
		}

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
						player.x = map.P_Start_Loc[0].x;
						player.y = map.P_Start_Loc[0].y;
						player.DOWN = false;
						player.is_in_air = false;
						window_move = true;

						player.player_life--;
					}
				}
			}
		}
		
		for (int i = 0; i < map.enemy_count; i++)
		{
			Move_Enemy(&map.enemys[i], map.blocks[map.enemys[i].on_block], 3);
			Update_Enemy_rect(&map.enemys[i]);
		}

		if (window_move)
		{
			MoveWindow(hWnd, std::clamp(player.x - (wnd_rt.right / 2),(long)0, (long)Desk_rect.right - (wnd_rt.right)), player.y - (wnd_rt.bottom / 2), 320, 320, true);
		}
		else
		{
			MoveWindow(hWnd, window_rect.left, player.y - (wnd_rt.bottom / 2), 320, 320, true);
		}

		
		if (map.boss_count != 0)
		{
			if (map.boss.attack_time <= 0)
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
					player.x = map.P_Start_Loc[0].x;
					player.y = map.P_Start_Loc[0].y;
					player.DOWN = false;
					player.is_in_air = false;
					window_move = true;
					player.player_life++;
				}
				else
				{
					player.x = map.P_Start_Loc[0].x;
					player.y = map.P_Start_Loc[0].y;
					player.DOWN = false;
					player.is_in_air = false;
					window_move = true;

					player.player_life--;
				}
			}

			for (int i = 0; i < map.block_count; i++)
			{
				if (player.x <= map.blocks[i].Block_rt.right && player.x >= map.blocks[i].Block_rt.left)
				{
					if (player.y + Size >= map.blocks[i].Block_rt.top && player.y + Size < map.blocks[i].y)
					{
						player.x--;
					}
				}

				if (map.blocks[i].x <= Desk_rect.left)
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
			map.P_Start_Loc[0].x = map.blocks[0].x;
			map.P_Start_Loc[0].y = map.blocks[0].Block_rt.top - Size;
		}

		client.SendPlayerUpdatePacket(&player);
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

LRESULT CALLBACK WndEditProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) {
	HDC hdc, mdc, resourcedc;
	HBITMAP hBitmap;
	static HBITMAP Character_bitmap, Enemy_bitmap, Object_bitmap, Platforms_bitmap, BGM_bitmap, BGN_bitmap, Tino_bitmap, Enemy_rv_bitmap;
	PAINTSTRUCT ps;

	static DrawMod curDrawmod;

	CS_UploadMapPacket UMP;

	static int start_x, start_y, old_x, old_y;
	int end_x, end_y;
	static bool down;

	switch (iMessage) {
	case WM_CREATE:
		srand(time(NULL));
		Character_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP4));
		Enemy_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP1));
		Enemy_rv_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP14));
		Object_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP2));
		Platforms_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP3));
		BGM_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP10));
		BGN_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP11));
		Tino_bitmap = LoadBitmap(g_hinst, MAKEINTRESOURCE(IDB_BITMAP9));
		down = false;
		curDrawmod = D_Block;
		SetTimer(hWnd, 0, 0.016, (TIMERPROC)StartTimer);
		break;
	case WM_LBUTTONDOWN:
		if (client.my_player_id != 0) break;

		start_y = old_y = HIWORD(lParam);
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
			Editmap.boss_count = 1;
			break;
		case D_player:
			Editmap.P_Start_Loc[0].x = start_x;
			Editmap.P_Start_Loc[0].y = start_y;

			Editmap.P_Start_Loc[1].x = start_x;
			Editmap.P_Start_Loc[1].y = start_y;

			Editmap.P_Start_Loc[2].x = start_x;
			Editmap.P_Start_Loc[2].y = start_y;
			player = Make_Player(Editmap.P_Start_Loc[0].x, Editmap.P_Start_Loc[0].y);
			break;
		default:
			break;
		}
		break;
	case WM_MOUSEMOVE:
		if (client.my_player_id != 0) break;

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
		if (client.my_player_id != 0) break;

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
			Editmap.P_Start_Loc[0].x = 0;
			Editmap.P_Start_Loc[0].y = 0;
			player = Make_Player(Editmap.P_Start_Loc[0].x, Editmap.P_Start_Loc[0].y);
			break;
		default:
			break;
		}
		break;
	case WM_CHAR:
		if (client.my_player_id != 0) break;
		switch (wParam)
		{
		case 's': case 'S':
			map = Editmap;
			selected_map = 99;
			CloseEditWindow(hWnd);

			UMP.Init(map);
			client.SendUploadMapPacket(&UMP);
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

		SelectObject(resourcedc, Tino_bitmap);
		TransparentBlt(mdc, Editmap.P_Start_Loc[0].x - Size, Editmap.P_Start_Loc[0].y - Size, (Size * 2), (Size * 2), resourcedc, 0, 0, 150, 140, RGB(0, 0, 255));

		Draw_Map(&mdc, &resourcedc, Object_bitmap, Platforms_bitmap, Enemy_bitmap, Enemy_rv_bitmap, Editmap);

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