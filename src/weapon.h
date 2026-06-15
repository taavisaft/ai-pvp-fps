#pragma once

// Per-weapon stats. One active weapon for now (Uzi); the table is what lets the
// game grow to multiple weapons (AR / SMG / sniper) later. game.h aliases the old
// per-weapon constants to gWeapon's fields so existing call sites keep working.
struct WeaponDef {
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
    float dragK;           // quadratic air drag coefficient (0 = none)
    float falloffStart;    // m — damage is full up to here
    float falloffEnd;      // m — damage reaches falloffMin here
    float falloffMin;      // damage scale at/after falloffEnd (1.0 = no falloff)
};

// Uzi — 9mm SMG. ~400 m/s muzzle, 32-round mag, ~600 rpm, light damage + falloff.
inline constexpr WeaponDef UZI = {
    /*muzzleVel*/      400.0f,
    /*ttl*/           3.0f,
    /*dmg*/           18.0f,
    /*magSize*/       32,
    /*reservePerLife*/ 64,
    /*reloadTime*/    2.6f,
    /*fireSemiInt*/   0.12f,
    /*fireBurstInt*/  0.07f,
    /*fireAutoInt*/   0.10f,   // ~600 rpm
    /*burstCount*/    3,
    /*dragK*/         0.0009f,
    /*falloffStart*/  30.0f,
    /*falloffEnd*/    120.0f,
    /*falloffMin*/    0.6f,
};

// Active weapon. Single global for now; make this runtime-mutable when adding
// weapon pickups / loadouts.
inline constexpr WeaponDef gWeapon = UZI;
