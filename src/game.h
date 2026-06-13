#pragma once
#include <cstdint>
#include <glm/glm.hpp>

constexpr int   MAX_PLAYERS    = 16;
constexpr int   MAX_BULLETS    = 256;    // global active pool (server-side cap)
constexpr int   AMMO_PER_LIFE  = 20;
constexpr int   PLAYER_HP      = 100;
constexpr float RESPAWN_TIME   = 3.0f;   // seconds dead before respawn
constexpr float BULLET_SPEED   = 50.0f;  // m/s
constexpr float BULLET_TTL     = 3.0f;   // seconds
constexpr float MOVE_SPEED     = 5.0f;   // m/s
constexpr float SPRINT_SPEED   = 8.0f;   // m/s, shift held
constexpr float JUMP_SPEED      = 6.0f;   // m/s upward; ~1.8 m peak, clears crates
constexpr float CROUCH_SPEED   = 2.5f;   // m/s, left-ctrl held
constexpr float STAND_HEIGHT   = 2.0f;   // full body height
constexpr float CROUCH_HEIGHT  = 1.2f;   // crouched body height (hitbox + render)
constexpr float GRAVITY        = 9.8f;   // m/s²
constexpr float BULLET_DMG     = 25;     // HP per hit
constexpr int   NET_HZ         = 20;     // state sync rate
constexpr int   PHYS_HZ        = 60;     // physics tick rate
constexpr int   UDP_PORT       = 7777;

constexpr float EYE_HEIGHT     = 1.7f;
constexpr float CROUCH_EYE     = 1.0f;   // camera height while crouched

struct Player {
    glm::vec3 pos          = {0, 0, 0};  // feet; Y rises when jumping
    float     velY         = 0.0f;       // vertical velocity (jump/gravity)
    float     airVX        = 0.0f;       // horizontal velocity locked at takeoff
    float     airVZ        = 0.0f;       // (no mid-air steering)
    bool      crouched     = false;      // affects height, hitbox, speed
    float     yaw          = 0.0f;       // degrees
    int       hp           = PLAYER_HP;
    int       ammo         = AMMO_PER_LIFE;
    int       kills        = 0;          // persists across respawns
    int       deaths       = 0;
    bool      alive        = true;
    float     respawnTimer = 0.0f;       // counts down while dead
};

struct Bullet {
    glm::vec3 pos;
    glm::vec3 vel;
    float     lifetime = 0.0f;
    int       ownerID  = -1;      // 0..MAX_PLAYERS-1
    bool      active   = false;
};

struct GameState {
    Player   players[MAX_PLAYERS];
    Bullet   bullets[MAX_BULLETS];
    int      bulletCount = 0;
    uint16_t usedMask    = 0;     // bit i set = slot i occupied
};

struct InputState {
    bool  w, a, s, d;
    bool  shoot;                   // true on press, not hold
    bool  sprint;                  // shift held
    bool  jump;                    // space held
    bool  crouch;                  // left-ctrl held
    float yaw;
    float pitch;
};
