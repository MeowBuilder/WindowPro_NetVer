#pragma once

// Dialog Box ID
#define IDD_IP_INPUT                102

// Controls IDs
#define IDC_STATIC_IP_PROMPT        1001
#define IDC_IPADDRESS_EDIT          1002
#define IDOK_CONNECT                1003
#define IDCANCEL_CONNECT            1004
#define IDC_PLAYER_LIST             1005 // ListBox for displaying connected players
#define IDOK_START_GAME             1006 // Button to start the game

// Custom Messages
#define WM_USER_UPDATE_PLAYER_LIST  (WM_USER + 1)
