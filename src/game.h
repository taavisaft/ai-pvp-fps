#pragma once
#include <cstdint>
#include <glm/glm.hpp>

constexpr int   MAX_PLAYERS    = 16;
constexpr int   MAX_BULLETS    = 256;    // global active pool (server-side cap)
constexpr int   MAG_SIZE        = 10;    // rounds per magazine
constexpr int   RESERVE_PER_LIFE = 30;   // spare rounds, refilled on respawn
constexpr float RELOAD_TIME     = 2.0f;  // seconds to reload
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

// Weapon: aim-down-sights vs hipfire (PUBG-style)
constexpr float HIP_FOV        = 75.0f;  // degrees
constexpr float ADS_FOV        = 55.0f;  // zoomed in
// The muzzle fires along the crosshair ray, so a stationary first shot is dead-on
// at any range (minus drop). All inaccuracy comes from recoil and movement, not a
// base cone, so both hip and ADS start at zero spread.
constexpr float HIP_SPREAD_DEG = 0.0f;
constexpr float ADS_SPREAD_DEG = 0.0f;
constexpr float ADS_LERP_SPEED = 12.0f;  // viewmodel/FOV transition rate

// Movement accuracy penalties (spread bloom added on top of the base, in degrees).
// Stationary stays precise; moving/sprinting/airborne widen the cone, crouching
// tightens it, and aiming down sights strongly reduces the movement penalty.
constexpr float MOVE_SPREAD_DEG    = 2.0f;   // walking
constexpr float SPRINT_SPREAD_DEG  = 4.5f;   // sprinting while moving
constexpr float JUMP_SPREAD_DEG    = 8.0f;   // airborne / falling
constexpr float CROUCH_SPREAD_MULT = 0.5f;   // crouched + stationary tightens base
constexpr float ADS_MOVE_MULT      = 0.35f;  // ADS reduces the movement penalty

constexpr float HIT_MARKER_TIME = 0.5f;  // seconds the hit marker stays visible

// Recoil (client-side camera kick; the recoiled aim is what gets shot). Per-shot
// vertical climb + small random horizontal, escalating over a sustained spray,
// recovering back to the player's aim once firing stops. All tunable.
constexpr float RECOIL_PITCH_MIN    = 0.35f;  // deg up per shot, cold
constexpr float RECOIL_PITCH_MAX    = 0.85f;  // deg up per shot, fully heated
constexpr float RECOIL_YAW          = 0.18f;  // deg, base horizontal random magnitude
constexpr float RECOIL_HEAT_CAP     = 10.0f;  // shots until kick fully ramped
constexpr float RECOIL_HEAT_RESET   = 0.25f;  // s of no fire before heat resets
constexpr float RECOIL_HIP_MULT     = 1.5f;   // hipfire kicks harder
constexpr float RECOIL_ADS_MULT     = 0.85f;  // aiming is steadier
constexpr float RECOIL_RECOVER_DELAY = 0.12f; // s after last shot before recovery
                                              // (> FIRE_AUTO_INT so a spray climbs cleanly)
constexpr float RECOIL_RECOVER_TAU  = 0.10f;  // exp decay time constant of recovery
constexpr float RECOIL_PITCH_CAP    = 25.0f;  // max accumulated upward offset
constexpr float RECOIL_FIRST_MULT   = 1.25f;  // extra kick on the first (cold) shot

// Fire modes (client-side only — decides when shots are registered)
enum FireMode { FIRE_SEMI = 0, FIRE_BURST, FIRE_AUTO, FIRE_MODE_COUNT };
constexpr float FIRE_SEMI_INT  = 0.12f;  // min seconds between shots, semi
constexpr float FIRE_BURST_INT = 0.07f;  // within a burst
constexpr float FIRE_AUTO_INT  = 0.10f;  // ~600 rpm full-auto
constexpr int   BURST_COUNT    = 3;

struct Player {
    glm::vec3 pos          = {0, 0, 0};  // feet; Y rises when jumping
    float     velY         = 0.0f;       // vertical velocity (jump/gravity)
    float     airVX        = 0.0f;       // horizontal velocity locked at takeoff
    float     airVZ        = 0.0f;       // (no mid-air steering)
    bool      crouched     = false;      // affects height, hitbox, speed
    float     yaw          = 0.0f;       // degrees
    int       hp           = PLAYER_HP;
    int       mag          = MAG_SIZE;       // rounds in magazine
    int       reserve      = RESERVE_PER_LIFE; // spare rounds
    float     reloadTimer  = 0.0f;           // >0 while reloading (server sim)
    bool      reloading    = false;          // for HUD/clients (mirrors reloadTimer>0)
    int       kills        = 0;          // persists across respawns
    int       deaths       = 0;
    bool      alive        = true;
    float     respawnTimer = 0.0f;       // counts down while dead
    int       hits         = 0;          // damaging hits dealt (for hit markers)
    glm::vec3 lastHitPos    = {0, 0, 0}; // world impact point of the latest hit
};

struct Bullet {
    glm::vec3 pos;
    glm::vec3 vel;
    float     lifetime   = 0.0f;
    int       ownerID    = -1;     // 0..MAX_PLAYERS-1
    bool      active     = false;
    float     compRewind = 0.0f;   // server-only: seconds to rewind targets for hits
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
    bool  shootHeld;               // left-mouse held (for full-auto)
    bool  sprint;                  // shift held
    bool  jump;                    // space held
    bool  crouch;                  // left-ctrl held
    bool  ads;                     // right-mouse held (aim down sights)
    bool  reload;                  // R held
    float yaw;
    float pitch;
};
