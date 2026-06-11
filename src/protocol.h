#pragma once
#include <cstdint>

enum PacketType : uint8_t {
    PKT_HELLO  = 1,   // client → server: request to join
    PKT_ACCEPT = 2,   // server → client: player ID assigned
    PKT_INPUT  = 3,   // client → server: this frame's input
    PKT_STATE  = 4,   // server → client: full authoritative state
    PKT_BYE    = 6,   // either direction: clean disconnect
};

#pragma pack(push, 1)

struct HelloPacket  { PacketType type; };   // PKT_HELLO
struct AcceptPacket { PacketType type; uint8_t playerID; };  // 0 or 1

struct InputPacket {
    PacketType type;   // PKT_INPUT
    uint32_t   seq;    // monotonically increasing, for drop detection
    uint8_t    keys;   // bitmask: W=1 A=2 S=4 D=8 SHOOT=16
    float      yaw;
    float      pitch;
};

struct BulletNetState { float x, y, z; };

struct StatePacket {
    PacketType    type;          // PKT_STATE
    uint32_t      seq;
    float         p0x, p0y, p0z, p0yaw; int32_t p0hp;
    float         p1x, p1y, p1z, p1yaw; int32_t p1hp;
    uint8_t       bulletCount;
    BulletNetState bullets[16];
    uint8_t       gameOver;
    int8_t        winnerID;     // -1, 0, or 1
};

struct ByePacket { PacketType type; };

#pragma pack(pop)

// keys bitmask bits
constexpr uint8_t KEY_W     = 1;
constexpr uint8_t KEY_A     = 2;
constexpr uint8_t KEY_S     = 4;
constexpr uint8_t KEY_D     = 8;
constexpr uint8_t KEY_SHOOT = 16;
