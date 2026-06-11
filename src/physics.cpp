#include "physics.h"
#include "map.h"
#include <cmath>

bool aabbHit(glm::vec3 p, glm::vec3 playerPos) {
    glm::vec3 center = playerPos + glm::vec3(0, 1.0f, 0);
    glm::vec3 half   = {0.4f, 1.0f, 0.4f};
    return fabsf(p.x - center.x) < half.x &&
           fabsf(p.y - center.y) < half.y &&
           fabsf(p.z - center.z) < half.z;
}

glm::vec3 dirFromYawPitch(float yaw, float pitch) {
    return glm::normalize(glm::vec3(
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch))
    ));
}

static bool pointInBox(glm::vec3 p, const Box& b) {
    return fabsf(p.x - b.center.x) < b.half.x &&
           fabsf(p.y - b.center.y) < b.half.y &&
           fabsf(p.z - b.center.z) < b.half.z;
}

// Push the player's XZ footprint (half extent 0.4) out of every map box,
// along the axis of least overlap. Player is grounded, boxes sit on the
// ground, so Y never separates them — XZ only.
static void collideWithMap(Player& p) {
    constexpr float R = 0.4f;
    for (int i = 0; i < MAP_BOX_COUNT; i++) {
        const Box& b = MAP_BOXES[i];
        float dx = (b.half.x + R) - fabsf(p.pos.x - b.center.x);
        float dz = (b.half.z + R) - fabsf(p.pos.z - b.center.z);
        if (dx <= 0.0f || dz <= 0.0f) continue;
        if (dx < dz) p.pos.x += (p.pos.x < b.center.x) ? -dx : dx;
        else         p.pos.z += (p.pos.z < b.center.z) ? -dz : dz;
    }
    if (p.pos.x >  ARENA_HALF) p.pos.x =  ARENA_HALF;
    if (p.pos.x < -ARENA_HALF) p.pos.x = -ARENA_HALF;
    if (p.pos.z >  ARENA_HALF) p.pos.z =  ARENA_HALF;
    if (p.pos.z < -ARENA_HALF) p.pos.z = -ARENA_HALF;
}

void movePlayer(Player& p, const InputState& in, float dt) {
    glm::vec3 forward = glm::normalize(glm::vec3(
        cos(glm::radians(in.yaw)), 0, sin(glm::radians(in.yaw))));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    if (in.w) p.pos += forward * MOVE_SPEED * dt;
    if (in.s) p.pos -= forward * MOVE_SPEED * dt;
    if (in.a) p.pos -= right   * MOVE_SPEED * dt;
    if (in.d) p.pos += right   * MOVE_SPEED * dt;
    p.pos.y = 0.0f; // lock to ground
    p.yaw = in.yaw;
    collideWithMap(p);
}

bool spawnBullet(GameState& gs, const glm::vec3& eyePos, const glm::vec3& dir, int ownerID) {
    Player& owner = gs.players[ownerID];
    if (!owner.alive || owner.ammo <= 0) return false;
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet& b = gs.bullets[i];
        if (b.active) continue;
        b.pos      = eyePos;
        b.vel      = dir * BULLET_SPEED;
        b.lifetime = BULLET_TTL;
        b.ownerID  = ownerID;
        b.active   = true;
        owner.ammo--;
        return true;
    }
    return false;
}

void updateBullets(GameState& gs, float dt) {
    int active = 0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet& b = gs.bullets[i];
        if (!b.active) continue;

        b.vel.y    -= GRAVITY * dt;
        b.pos      += b.vel * dt;
        b.lifetime -= dt;
        if (b.lifetime <= 0.0f) { b.active = false; continue; }
        if (b.pos.y <= 0.0f) { b.active = false; continue; }  // hit the ground

        for (int m = 0; m < MAP_BOX_COUNT; m++) {
            if (pointInBox(b.pos, MAP_BOXES[m])) { b.active = false; break; }
        }
        if (!b.active) continue;

        for (int pid = 0; pid < MAX_PLAYERS; pid++) {
            if (pid == b.ownerID) continue;
            if (!(gs.usedMask & (1u << pid))) continue;
            Player& target = gs.players[pid];
            if (!target.alive) continue;
            if (!aabbHit(b.pos, target.pos)) continue;

            target.hp -= (int)BULLET_DMG;
            b.active = false;
            if (target.hp <= 0) {
                target.hp           = 0;
                target.alive        = false;
                target.respawnTimer = RESPAWN_TIME;
            }
            break;
        }
        if (b.active) active++;
    }
    gs.bulletCount = active;
}
