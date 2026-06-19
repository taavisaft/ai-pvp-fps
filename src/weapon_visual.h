#pragma once
#include <glm/glm.hpp>
#include "weapon.h"   // WeaponId / WEP_COUNT

// Client-only (uses glm): one canonical per-weapon row of hand/muzzle anchors, so the
// first-person viewmodel and the third-person skeleton both place hands from data
// instead of values scattered through the renderers. Adding a weapon = add a row; a
// longer gun pushes the off hand and muzzle out via the larger +Z anchors. The two
// views draw the gun at genuinely different placements, so each carries its own grip
// (tp* / fp*) — but every weapon's grips live in a single place.
//
// Frames (both: +X right, +Y up, +Z barrel-forward, meters):
//   tp* — third-person "hold" frame, mounted at the chest.
//   fp* — first-person viewmodel "anchor" frame at the camera.
struct WeaponVisual {
    glm::vec3 tpFireGrip;     // 3rd-person trigger-hand grip
    glm::vec3 tpSupportGrip;  // 3rd-person off-hand fore-grip; +Z grows with length
    glm::vec3 fpFireGrip;     // 1st-person fist center on the grip
    glm::vec3 fpMuzzle;       // 1st-person muzzle (flash / future tracer); +Z = length
};

// Uzi — boxy SMG, long barrel: off hand reaches well forward, muzzle far out.
inline constexpr WeaponVisual UZI_VIS = {
    /*tpFireGrip*/    {0.0f, -0.06f, 0.04f},
    /*tpSupportGrip*/ {0.0f,  0.00f, 0.26f},
    /*fpFireGrip*/    {0.012f, -0.09f, -0.02f},
    /*fpMuzzle*/      {0.0f,  0.02f, 0.36f},
};

// Glock 19 — compact pistol: off hand close in, short muzzle.
inline constexpr WeaponVisual GLOCK19_VIS = {
    /*tpFireGrip*/    {0.0f, -0.06f, 0.04f},
    /*tpSupportGrip*/ {0.0f,  0.00f, 0.13f},
    /*fpFireGrip*/    {0.012f, -0.10f, -0.05f},
    /*fpMuzzle*/      {0.0f,  0.02f, 0.26f},
};

inline const WeaponVisual WEAPON_VIS[WEP_COUNT] = { UZI_VIS, GLOCK19_VIS };

inline const WeaponVisual& weaponVisual(uint8_t id) {
    return WEAPON_VIS[id < WEP_COUNT ? id : WEP_UZI];
}
