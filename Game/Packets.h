#ifndef PACKETS_H
#define PACKETS_H

#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#pragma comment(lib, "ws2_32.lib")

// --- 사용자 정의 자료구조 ---
// 프로젝트 계획서에 정의된 좌표 구조체
struct Point
{
    int x;
    int y;
};
// --- 패킷 타입 정의 ---
// Client -> Server
constexpr char CS_UPLOAD_MAP = 0;
constexpr char CS_START_SESSION_REQ = 1;
constexpr char CS_PLAYER_UPDATE = 2;
constexpr char CS_END_SESSION_REQ = 3;

// Server -> Client
constexpr char SC_ASSIGN_ID = 10;
constexpr char SC_MAP_UPLOAD_RSP = 11;
constexpr char SC_MAP_INFO = 12;
constexpr char SC_GAME_STATE = 13;
constexpr char SC_EVENT = 14;
constexpr char SC_DISCONNECT = 15;

// --- 이벤트 타입 정의 (프로젝트 계획서 기반) ---
enum E_EventType { STAGE_CLEAR, GAME_WIN };

/*
 * --- 패킷 클래스 사용 가이드 ---
 *
 * 1. 송신 (데이터 보내기)
 *    - 보낼 패킷 객체를 생성합니다. (예: SC_AssignIDPacket packet(player_id);)
 *    - packet.Encode(); 메소드를 호출하여 모든 멤버를 네트워크 바이트 순서로 변환합니다.
 *    - send() 함수를 사용하여 패킷을 전송합니다.
 *      - send(socket, (char*)&packet, packet.size, 0);
 *
 * 2. 수신 (데이터 받기)
 *    - recv() 함수를 사용하여 char 배열(버퍼)에 데이터를 받습니다.
 *    - 버퍼의 주소를 BasePacket 포인터로 형변환하여 type을 확인합니다.
 *      - BasePacket* base_p = (BasePacket*)buffer;
 *    - type에 따라 적절한 패킷 타입의 포인터로 다시 형변환합니다.
 *      - if (base_p->type == SC_ASSIGN_ID) { SC_AssignIDPacket* p = (SC_AssignIDPacket*)buffer; ... }
 *    - p->Decode(); 메소드를 호출하여 모든 멤버를 호스트 바이트 순서로 변환합니다.
 *    - 이제 패킷의 멤버 변수(p->player_id 등)를 안전하게 사용할 수 있습니다.
 *
 * 3. 디버깅
 *    - 송신 또는 수신 후 p->Log(); 메소드를 호출하여 패킷의 현재 내용을 콘솔에 출력할 수 있습니다.
 */

// --- 패킷 구조체 정의 ---
#pragma pack(push, 1) // 네트워크 패킷 직렬화를 위해 구조체 패딩을 1바이트로 설정

// 모든 패킷의 기반이 되는 클래스
class BasePacket {
public:
    size_t size; // 패킷의 전체 크기
    char type;   // 패킷의 종류

    // 생성자
    BasePacket() : size(0), type(0) {} // 실제 크기와 타입은 파생 클래스에서 설정

    // 엔디안 변환 및 로깅을 위한 가상 메소드
    virtual void Encode() {} // 송신 전 호출 (네트워크 바이트 순서로 변환)
    virtual void Decode() {} // 수신 후 호출 (호스트 바이트 순서로 변환)
    virtual void Log() const {
        printf("[BasePacket] Type: %d, Size: %zu\n", type, size);
    }
    // 파생 클래스의 소멸자가 올바르게 호출되도록 가상 소멸자 선언
    virtual ~BasePacket() = default;
};

// [S->C] 서버가 클라이언트에게 고유 ID를 할당하는 패킷
// [전송 시점]: 서버가 클라이언트의 접속을 수락(accept)한 직후,
//             해당 클라이언트에게 플레이어 ID(예: 0, 1)를 부여하기 위해 한 번만 전송합니다.
class SC_AssignIDPacket : public BasePacket {
public:
    u_short player_id; // 플레이어에게 할당될 고유 ID

    // 기본 생성자
    SC_AssignIDPacket() {
        size = sizeof(SC_AssignIDPacket);
        type = SC_ASSIGN_ID;
        player_id = (u_short)-1; // 기본값으로 유효하지 않은 ID (0xFFFF) 설정
    }

    // ID를 인자로 받는 생성자
    SC_AssignIDPacket(u_short id) {
        size = sizeof(SC_AssignIDPacket);
        type = SC_ASSIGN_ID;
        player_id = id;
    }

    // 네트워크 바이트 순서로 변환 (송신용)
    void Encode() override {
        player_id = htons(player_id);
    }

    // 호스트 바이트 순서로 변환 (수신용)
    void Decode() override {
        player_id = ntohs(player_id);
    }

    // 디버깅용 로그 출력
    void Log() const override {
        BasePacket::Log();
        printf("  Player ID: %hu\n", player_id);
    }
};

// [S->C] 맵 업로드 결과를 클라이언트에게 응답하는 패킷
// [전송 시점]: 클라이언트가 CS_UPLOAD_MAP 패킷을 통해 맵 데이터를 서버에 전송한 후,
//             서버가 해당 맵 데이터의 유효성 검사 및 처리를 완료했을 때 클라이언트에게 응답으로 전송합니다.
class SC_MapUploadResponsePacket : public BasePacket {
public:
    bool is_success; // 맵 업로드 성공 여부 (true: 성공, false: 실패)

    // 기본 생성자
    SC_MapUploadResponsePacket() {
        size = sizeof(SC_MapUploadResponsePacket);
        type = SC_MAP_UPLOAD_RSP;
        is_success = false; // 기본값은 실패
    }

    // 성공 여부를 인자로 받는 생성자
    SC_MapUploadResponsePacket(bool success) {
        size = sizeof(SC_MapUploadResponsePacket);
        type = SC_MAP_UPLOAD_RSP;
        is_success = success;
    }

    // bool 타입은 1바이트이므로 엔디안 변환이 필요 없음
    void Encode() override {
        // is_success는 1바이트 bool이므로 변환 불필요
    }

    void Decode() override {
        // is_success는 1바이트 bool이므로 변환 불필요
    }

    // 디버깅용 로그 출력
    void Log() const override {
        BasePacket::Log();
        printf("  Map Upload Success: %s\n", is_success ? "true" : "false");
    }
};

#pragma pack(pop)

#endif // PACKETS_H