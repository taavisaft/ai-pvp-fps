#pragma once
#include "game.h"

// Returns true if point p is inside the player's AABB (shorter when crouched)
bool aabbHit(glm::vec3 p, glm::vec3 playerPos, bool crouched);

// Look direction from yaw/pitch in degrees (matches camera front)
glm::vec3 dirFromYawPitch(float yaw, float pitch);

// Authoritative shot: fills bullet origin + direction for a shot from `eye`.
// ADS = spawn from eye, tight spread; hipfire = spawn from barrel muzzle
// (lower-right of view) with a wide random cone. Uses rand() for spread.
void weaponShot(float yaw, float pitch, const glm::vec3& eye, bool ads,
                glm::vec3& outOrigin, glm::vec3& outDir);

// WASD movement on XZ plane, Y locked to 0.
// Includes collision: pushed out of map boxes, clamped to arena bounds.
void movePlayer(Player& p, const InputState& in, float dt);

// Spawns a bullet from the pool; returns false if pool exhausted / no ammo.
// compRewind (server lag comp): seconds to rewind targets when testing this
// bullet's hits, so it collides with where the shooter saw them.
bool spawnBullet(GameState& gs, const glm::vec3& eyePos, const glm::vec3& dir, int ownerID,
                 float compRewind = 0.0f);

// Reload state machine for one player (start/finish, mag from reserve). Run per tick.
void updateReload(Player& p, bool wantReload, float dt);

// Looks up a player's historical hitbox `rewindSec` seconds in the past. Returns
// false if the player wasn't present/alive then (skip the hit). Used by the server
// for lag compensation; ctx is the server's position history.
typedef bool (*RewindLookup)(const void* ctx, int pid, float rewindSec,
                             glm::vec3& pos, bool& crouched, bool& alive);

// Integrates bullets (gravity, TTL), checks hits vs players, applies damage/kills.
// lookup == nullptr: hits tested against current positions (offline/client).
// lookup != nullptr: hits tested against rewound positions (server lag comp).
void updateBullets(GameState& gs, float dt,
                   RewindLookup lookup = nullptr, const void* ctx = nullptr);
