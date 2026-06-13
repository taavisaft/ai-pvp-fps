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
constexpr float GRAVITY        = 9.8f;   // m/s²
constexpr float BULLET_DMG     = 25;     // HP per hit
constexpr int   NET_HZ         = 20;     // state sync rate
constexpr int   PHYS_HZ        = 60;     // physics tick rate
constexpr int   UDP_PORT       = 7777;

constexpr float EYE_HEIGHT     = 1.7f;

struct Player {
    glm::vec3 pos          = {0, 0, 0};  // feet, Y always 0
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
    float yaw;
    float pitch;
};
