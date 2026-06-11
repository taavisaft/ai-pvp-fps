#pragma once
#include "game.h"

// Returns true if point p is inside the player's AABB
bool aabbHit(glm::vec3 p, glm::vec3 playerPos);

// Look direction from yaw/pitch in degrees (matches camera front)
glm::vec3 dirFromYawPitch(float yaw, float pitch);

// WASD movement on XZ plane, Y locked to 0.
// Includes collision: pushed out of map boxes, clamped to arena bounds.
void movePlayer(Player& p, const InputState& in, float dt);

// Spawns a bullet from the pool; returns false if pool exhausted
bool spawnBullet(GameState& gs, const glm::vec3& eyePos, const glm::vec3& dir, int ownerID);

// Integrates bullets (gravity, TTL), checks hits vs both players,
// applies damage and sets gameOver/winnerID
void updateBullets(GameState& gs, float dt);
