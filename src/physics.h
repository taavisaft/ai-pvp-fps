#pragma once
#include "game.h"

// Returns true if point p is inside the player's AABB (shorter when crouched)
bool aabbHit(glm::vec3 p, glm::vec3 playerPos, bool crouched);

// A regional hitbox + damage multiplier (head 2x, torso 1x, legs 0.8x). An oriented
// box: `M` maps the box's local frame to world (box spans ±half around M's origin),
// `invM` is the cached inverse for sweeps. To test, transform the segment by invM and
// run segmentAabb against `half` at the origin; to draw, use M * scale(2*half).
struct HitRegion { glm::mat4 M; glm::mat4 invM; glm::vec3 half; float mult; };

// Fills the player's hit regions from buildPlayerPose (playerpose.h) — the exact
// oriented boxes the client renders (FK spine, IK arms + weapon, IK legs), each tagged
// with its damage multiplier, plus ~1 cm pad (legs a bit more: their walk swing is
// client-side animation). Depends only on pos + crouched + yaw + pitch + lean + ads +
// weaponId — all carried in the lag-comp rewind snapshot — so server hit tests match
// the rewound pose. Returns the count.
constexpr int MAX_HIT_REGIONS = 24;   // = MAX_POSE_BOXES
int playerHitRegions(const glm::vec3& pos, bool crouched, float yaw, float pitch,
                     float lean, bool ads, uint8_t weaponId,
                     HitRegion out[MAX_HIT_REGIONS]);

// Swept collision: does segment p0->p1 hit AABB(center, half)? tHit = entry point
// as a fraction [0,1] along the segment. Velocity-independent — used for bullets
// so fast rounds can't tunnel through cover or players in one tick.
bool segmentAabb(const glm::vec3& p0, const glm::vec3& p1,
                 const glm::vec3& center, const glm::vec3& half, float& tHit);

// Look direction from yaw/pitch in degrees (matches camera front)
glm::vec3 dirFromYawPitch(float yaw, float pitch);

// Spread cone half-angle (degrees) for a shot, from stance + movement: stationary
// is precise, moving/sprinting/airborne bloom, crouching tightens, ADS steadies.
float aimSpread(const Player& p, const InputState& in);

// Authoritative shot: fills bullet origin + direction for a shot from `eye`.
// ADS spawns from the eye, hipfire from the barrel muzzle (lower-right of view).
// spreadDeg is the random cone half-angle (see aimSpread); 0 = dead-on.
void weaponShot(float yaw, float pitch, const glm::vec3& eye, bool ads,
                float spreadDeg, glm::vec3& outOrigin, glm::vec3& outDir);

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
                             glm::vec3& pos, bool& crouched, float& yaw, float& pitch,
                             float& lean, bool& ads, uint8_t& weaponId, bool& alive);

// A bullet's resting point on world geometry + the surface normal it struck. Collected
// so callers can stamp decals (offline) or broadcast them to clients (server). Player
// hits and mid-air TTL expiries are not impacts.
struct Impact { glm::vec3 pos; glm::vec3 normal; };

// Integrates bullets (gravity, TTL), checks hits vs players, applies damage/kills.
// lookup == nullptr: hits tested against current positions (offline/client).
// lookup != nullptr: hits tested against rewound positions (server lag comp).
// outImpacts (optional): world impacts this call are appended starting at *outCount,
// up to maxImpacts; *outCount is advanced. Pass nullptr to skip impact collection.
void updateBullets(GameState& gs, float dt,
                   RewindLookup lookup = nullptr, const void* ctx = nullptr,
                   Impact* outImpacts = nullptr, int* outCount = nullptr, int maxImpacts = 0);
