#pragma once
#include <cstddef>
#include <cstdint>

enum PacketType : uint8_t {
    PKT_HELLO  = 1,   // client → server: request to join
    PKT_ACCEPT = 2,   // server → client: player ID assigned
    PKT_INPUT  = 3,   // client → server: this frame's input
    PKT_STATE  = 4,   // server → client: full authoritative state
    PKT_BYE    = 6,   // either direction: clean disconnect
};

// Max bullets carried per StatePacket; keeps the packet under typical MTU.
// Bullets beyond this are still simulated server-side, just not rendered.
constexpr int NET_MAX_BULLETS = 64;
constexpr int NET_MAX_PLAYERS = 16;   // must equal MAX_PLAYERS

#pragma pack(push, 1)

struct HelloPacket  { PacketType type; };   // PKT_HELLO
struct AcceptPacket { PacketType type; uint8_t playerID; };  // 0..15

struct InputPacket {
    PacketType type;    // PKT_INPUT
    uint32_t   seq;     // monotonically increasing, for drop detection
    uint8_t    keys;    // bitmask: W=1 A=2 S=4 D=8 (SHOOT no longer used to fire)
    float      yaw;
    float      pitch;
    uint32_t   shotSeq; // total shots fired this session; reliable event count
    uint8_t    flags;   // FLAG_* bitmask (separate from keys, which is full)
    uint32_t   viewSeq;  // StatePacket seq the client is interpolating from (lag comp)
    uint8_t    viewFrac; // interpolation alpha * 255 toward viewSeq+1 (lag comp)
    uint8_t    weaponId; // client's selected weapon (server adopts it authoritatively)
    int8_t     lean;     // smoothed lean * 127, -127 (left)..+127 (right); shot origin
};

struct PlayerNetState {
    float   x, y, z, yaw;
    int32_t hp;
    uint8_t alive;
    uint8_t mag;       // rounds in magazine
    uint8_t reserve;   // spare rounds
    uint8_t reloading; // 1 while reloading
    uint8_t kills;     // saturates at 255
    uint8_t deaths;
    uint8_t shotsFired; // authoritative spawned-shot counter (wraps naturally)
    uint8_t crouched;
};

struct BulletNetState {
    uint8_t poolIdx;   // server pool slot, stable across packets (interpolation key)
    uint8_t owner;
    float   x, y, z;
};

// Sent truncated: header + players + bulletCount + bulletCount * BulletNetState.
// recvHits/recvHit* are per-recipient: stamped with the destination player's own
// hit counter + last impact point just before each send, for their hit marker.
struct StatePacket {
    PacketType     type;          // PKT_STATE
    uint32_t       seq;
    uint16_t       usedMask;      // bit i set = player slot i occupied
    uint8_t        recvHits;      // recipient's damaging-hits-dealt counter
    float          recvHitX, recvHitY, recvHitZ;  // recipient's last impact point
    PlayerNetState players[NET_MAX_PLAYERS];
    uint8_t        bulletCount;
    BulletNetState bullets[NET_MAX_BULLETS];
};

struct ByePacket { PacketType type; };

#pragma pack(pop)

// Wire size of a StatePacket carrying `count` bullets
constexpr int statePacketSize(int count) {
    return (int)offsetof(StatePacket, bullets) + count * (int)sizeof(BulletNetState);
}

// keys bitmask bits
constexpr uint8_t KEY_W     = 1;
constexpr uint8_t KEY_A     = 2;
constexpr uint8_t KEY_S     = 4;
constexpr uint8_t KEY_D     = 8;
constexpr uint8_t KEY_SHOOT  = 16;
constexpr uint8_t KEY_SPRINT = 32;
constexpr uint8_t KEY_JUMP    = 64;
constexpr uint8_t KEY_CROUCH  = 128;
// InputPacket.flags bits
constexpr uint8_t FLAG_ADS    = 1;
constexpr uint8_t FLAG_RELOAD = 2;
