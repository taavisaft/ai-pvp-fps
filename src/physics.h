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

// Spawns a bullet from the pool; returns false if pool exhausted / no ammo
bool spawnBullet(GameState& gs, const glm::vec3& eyePos, const glm::vec3& dir, int ownerID);

// Reload state machine for one player (start/finish, mag from reserve). Run per tick.
void updateReload(Player& p, bool wantReload, float dt);

// Integrates bullets (gravity, TTL), checks hits vs both players,
// applies damage and sets gameOver/winnerID
void updateBullets(GameState& gs, float dt);
