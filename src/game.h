#pragma once
#include <cstdint>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "weapon.h"

constexpr int   MAX_PLAYERS    = 16;
constexpr int   MAX_BULLETS    = 256;    // global active pool (server-side cap)
// Per-weapon stats live in weapon.h and are looked up per player via weaponDef(id);
// there are no global weapon constants anymore (two weapons can differ).
constexpr int   PLAYER_HP      = 100;
constexpr float RESPAWN_TIME   = 3.0f;   // seconds dead before respawn
constexpr float MOVE_SPEED     = 5.0f;   // m/s
constexpr float SPRINT_SPEED   = 8.0f;   // m/s, shift held
constexpr float JUMP_SPEED      = 3.5f;   // m/s upward; ~0.6 m peak
constexpr float CROUCH_SPEED   = 2.5f;   // m/s, left-ctrl held
constexpr float STAND_HEIGHT   = 2.0f;   // full body height
constexpr float CROUCH_HEIGHT  = 1.2f;   // crouched body height (hitbox + render)
constexpr float GRAVITY        = 9.8f;   // m/s²
constexpr int   NET_HZ         = 20;     // state sync rate
constexpr int   PHYS_HZ        = 60;     // physics tick rate
constexpr int   UDP_PORT       = 7777;
constexpr int   LOBBY_SCAN_PORTS = 8;    // lobby probes UDP_PORT .. UDP_PORT+7

constexpr float EYE_HEIGHT     = 1.7f;
constexpr float CROUCH_EYE     = 1.0f;   // camera height while crouched

// Lean (Q/E): the upper body pivots around a hip point, so the head arcs out
// sideways (and dips slightly) and the view rolls — not a flat sideways slide.
// The same arc moves the authoritative shot origin so you shoot from the peek;
// the body hitbox stays put (classic corner-peek). All tunable.
constexpr float LEAN_ANGLE_DEG  = 22.0f; // upper-body tilt at full lean
constexpr float LEAN_PIVOT      = 0.95f; // m from eye down to the hip pivot
constexpr float LEAN_LERP_SPEED = 12.0f; // smoothing rate toward target lean

// Eye displacement produced by leaning: lateral arc + small vertical dip, from
// rotating the (pivot→eye) arm about the hip. Shared by camera + server so the
// shot origin matches the view. lateral is signed (>0 = lean right).
struct LeanShift { float lateral; float drop; };
inline LeanShift leanShift(float lean) {
    float a = lean * glm::radians(LEAN_ANGLE_DEG);
    return { LEAN_PIVOT * std::sin(a), LEAN_PIVOT * (1.0f - std::cos(a)) };
}

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
constexpr float RECOIL_FIRST_MULT   = 1.25f;  // extra kick on the first (cold) shot
constexpr float RECOIL_HEAT_OVER    = 0.25f;  // extra deg/shot per heat unit past ramp

// Fire modes (client-side only — decides when shots are registered). Per-weapon
// fire intervals / burst counts now come from weaponDef(id), not constants.
enum FireMode { FIRE_SEMI = 0, FIRE_BURST, FIRE_AUTO, FIRE_MODE_COUNT };

struct Player {
    glm::vec3 pos          = {0, 0, 0};  // feet; Y rises when jumping
    float     velY         = 0.0f;       // vertical velocity (jump/gravity)
    float     airVX        = 0.0f;       // horizontal velocity locked at takeoff
    float     airVZ        = 0.0f;       // (no mid-air steering)
    bool      crouched     = false;      // affects height, hitbox, speed
    float     yaw          = 0.0f;       // degrees
    float     pitch        = 0.0f;       // degrees; client-side render (aim tilt) only
    float     lean         = 0.0f;       // -1..+1; client-side render (peek pose) only
    bool      ads          = false;      // aiming; client-side render (raised-gun pose) only
    int       hp           = PLAYER_HP;
    uint8_t   weaponId     = WEP_UZI;        // current weapon (per-player)
    int       mag          = UZI.magSize;    // rounds in magazine (held weapon)
    int       reserve      = UZI.reservePerLife; // spare rounds (held weapon)
    // Per-weapon ammo so swapping preserves each gun's state instead of refilling.
    // mag/reserve above mirror the held weapon; the holstered weapons live here.
    int       magW[WEP_COUNT]     = { UZI.magSize, GLOCK19.magSize };
    int       reserveW[WEP_COUNT] = { UZI.reservePerLife, GLOCK19.reservePerLife };
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
    glm::vec3 origin     = {0, 0, 0}; // muzzle position, for distance damage falloff
    float     lifetime   = 0.0f;
    int       ownerID    = -1;     // 0..MAX_PLAYERS-1
    uint8_t   weaponId   = WEP_UZI; // weapon that fired it (damage/falloff/drag)
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
    bool  leanLeft;                // Q held
    bool  leanRight;               // E held
    float lean = 0.0f;             // smoothed lean -1 (left)..+1 (right), sent to server
    float yaw;
    float pitch;
    uint8_t weaponId = WEP_UZI;    // selected weapon (1/2 keys), sent to server
};

// Switch a player to weapon `id`: stash the held weapon's ammo, restore the
// target weapon's saved ammo. No refill — swapping is not a faster reload.
// A reload in progress is cancelled (it doesn't carry to the other gun).
// Used on selection (1/2) and server adoption. Respawn resets via Player{}.
inline void giveWeapon(Player& p, uint8_t id) {
    if (id >= WEP_COUNT) id = WEP_UZI;
    if (id == p.weaponId) return;            // already holding it; no-op
    p.magW[p.weaponId]     = p.mag;          // holster current weapon's state
    p.reserveW[p.weaponId] = p.reserve;
    p.weaponId    = id;
    p.mag         = p.magW[id];              // draw target weapon's saved state
    p.reserve     = p.reserveW[id];
    p.reloadTimer = 0.0f;
    p.reloading   = false;
}
