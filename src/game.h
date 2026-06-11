#pragma once
#include <glm/glm.hpp>

constexpr int   MAX_BULLETS  = 16;
constexpr int   PLAYER_HP    = 100;
constexpr float BULLET_SPEED = 50.0f;   // m/s
constexpr float BULLET_TTL   = 3.0f;    // seconds
constexpr float MOVE_SPEED   = 5.0f;    // m/s
constexpr float GRAVITY      = 9.8f;    // m/s²
constexpr float BULLET_DMG   = 25;      // HP per hit
constexpr int   NET_HZ       = 20;      // state sync rate
constexpr int   PHYS_HZ      = 60;      // physics tick rate
constexpr int   UDP_PORT     = 7777;

constexpr float EYE_HEIGHT   = 1.7f;

struct Player {
    glm::vec3 pos   = {0, 0, 0};  // feet, Y always 0
    float     yaw   = 0.0f;       // degrees
    int       hp    = PLAYER_HP;
    bool      alive = true;
};

struct Bullet {
    glm::vec3 pos;
    glm::vec3 vel;
    float     lifetime = 0.0f;
    int       ownerID  = -1;      // 0 or 1
    bool      active   = false;
};

struct GameState {
    Player players[2];
    Bullet bullets[MAX_BULLETS];
    int    bulletCount = 0;
    bool   gameOver    = false;
    int    winnerID    = -1;       // -1 = in progress
};

struct InputState {
    bool  w, a, s, d;
    bool  shoot;                   // true on press, not hold
    float yaw;
    float pitch;
};
