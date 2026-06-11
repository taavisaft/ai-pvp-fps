#include "physics.h"
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
}

bool spawnBullet(GameState& gs, const glm::vec3& eyePos, const glm::vec3& dir, int ownerID) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet& b = gs.bullets[i];
        if (b.active) continue;
        b.pos      = eyePos;
        b.vel      = dir * BULLET_SPEED;
        b.lifetime = BULLET_TTL;
        b.ownerID  = ownerID;
        b.active   = true;
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

        for (int pid = 0; pid < 2; pid++) {
            if (pid == b.ownerID) continue;
            Player& target = gs.players[pid];
            if (!target.alive) continue;
            if (!aabbHit(b.pos, target.pos)) continue;

            target.hp -= (int)BULLET_DMG;
            b.active = false;
            if (target.hp <= 0) {
                target.hp       = 0;
                target.alive    = false;
                gs.gameOver     = true;
                gs.winnerID     = 1 - pid;
            }
            break;
        }
        if (b.active) active++;
    }
    gs.bulletCount = active;
}
