#pragma once
#include <cstdint>

// Per-weapon stats. Weapons are per-player (Player.weaponId / Bullet.weaponId) so
// the server stays authoritative and client prediction matches. weaponDef(id) maps
// an id to its stats; gWeaponId is the local client's current selection (sent to
// the server in InputPacket).
struct WeaponDef {
    const char* name;
    float muzzleVel;       // m/s — real projectile speed (swept collision handles it)
    float ttl;             // s — bullet lifetime (range = ttl * muzzleVel)
    float dmg;             // HP per hit at point blank (before falloff)
    int   magSize;
    int   reservePerLife;  // spare rounds, refilled on respawn
    float reloadTime;      // s
    float fireSemiInt;     // min s between semi shots
    float fireBurstInt;    // s between rounds within a burst
    float fireAutoInt;     // s between full-auto rounds (RoF = 1/this)
    int   burstCount;      // rounds per burst
    bool  semiOnly;        // true = no burst/auto fire modes (pistols)
    float dragK;           // quadratic air drag coefficient (0 = none)
    float falloffStart;    // m — damage is full up to here
    float falloffEnd;      // m — damage reaches falloffMin here
    float falloffMin;      // damage scale at/after falloffEnd (1.0 = no falloff)
};

// Uzi — 9mm SMG. ~400 m/s muzzle, 32-round mag, ~600 rpm, light damage + falloff.
inline constexpr WeaponDef UZI = {
    /*name*/          "UZI",
    /*muzzleVel*/      400.0f,
    /*ttl*/           3.0f,
    /*dmg*/           18.0f,
    /*magSize*/       32,
    /*reservePerLife*/ 64,
    /*reloadTime*/    1.1f,
    /*fireSemiInt*/   0.12f,
    /*fireBurstInt*/  0.07f,
    /*fireAutoInt*/   0.10f,   // ~600 rpm
    /*burstCount*/    3,
    /*semiOnly*/      false,
    /*dragK*/         0.0009f,
    /*falloffStart*/  30.0f,
    /*falloffEnd*/    120.0f,
    /*falloffMin*/    0.6f,
};

// Glock 19 — 9mm semi-auto pistol. Real specs: ~375 m/s muzzle (4" barrel),
// 15-round mag, semi-only, fast reload. Punchier per shot than the Uzi to reward
// the slower, aimed fire.
inline constexpr WeaponDef GLOCK19 = {
    /*name*/          "GLOCK 19",
    /*muzzleVel*/      375.0f,
    /*ttl*/           3.0f,
    /*dmg*/           24.0f,
    /*magSize*/       15,
    /*reservePerLife*/ 45,
    /*reloadTime*/    1.1f,
    /*fireSemiInt*/   0.12f,   // trigger-limited (~8 rps max)
    /*fireBurstInt*/  0.12f,
    /*fireAutoInt*/   0.12f,
    /*burstCount*/    1,
    /*semiOnly*/      true,
    /*dragK*/         0.0009f,
    /*falloffStart*/  25.0f,
    /*falloffEnd*/    100.0f,
    /*falloffMin*/    0.55f,
};

enum WeaponId : uint8_t { WEP_UZI = 0, WEP_GLOCK19 = 1, WEP_COUNT };

inline const WeaponDef WEAPONS[WEP_COUNT] = { UZI, GLOCK19 };

inline const WeaponDef& weaponDef(uint8_t id) {
    return WEAPONS[id < WEP_COUNT ? id : WEP_UZI];
}

// The local client's currently selected weapon (keys 1/2). Sent to the server,
// which is authoritative; offline it drives the practice weapon directly.
inline uint8_t gWeaponId = WEP_UZI;
