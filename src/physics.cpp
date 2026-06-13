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

static constexpr float FOOT_R = 0.4f;  // player XZ footprint half-extent

// Highest surface the player can stand on under their footprint: arena floor (0)
// or a box top they are coming down onto. `fromY` is the height before this tick's
// fall — a box counts only if the player was at/above its top, so you land when
// crossing a top from above but don't snap onto one you're rising past.
static float supportHeight(const Player& p, float fromY) {
    float floor = 0.0f;
    for (int i = 0; i < MAP_BOX_COUNT; i++) {
        const Box& b = MAP_BOXES[i];
        if (fabsf(p.pos.x - b.center.x) >= b.half.x + FOOT_R) continue;
        if (fabsf(p.pos.z - b.center.z) >= b.half.z + FOOT_R) continue;
        float top = b.center.y + b.half.y;
        if (top > floor && fromY >= top - 0.05f) floor = top;
    }
    return floor;
}

// Push the footprint out of boxes along the axis of least overlap. Skipped for
// boxes whose top is at/below the feet — you stand on those instead of bumping.
static void collideXZ(Player& p) {
    for (int i = 0; i < MAP_BOX_COUNT; i++) {
        const Box& b = MAP_BOXES[i];
        float top = b.center.y + b.half.y;
        if (p.pos.y >= top - 0.05f) continue;  // on/above the box → no side push
        float dx = (b.half.x + FOOT_R) - fabsf(p.pos.x - b.center.x);
        float dz = (b.half.z + FOOT_R) - fabsf(p.pos.z - b.center.z);
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
    p.yaw = in.yaw;

    float fromY    = p.pos.y;
    bool  grounded = fromY <= supportHeight(p, fromY) + 0.001f;

    // horizontal: full control on the ground; in the air keep takeoff momentum
    if (grounded) {
        float speed = in.sprint ? SPRINT_SPEED : MOVE_SPEED;
        glm::vec3 vel(0.0f);
        if (in.w) vel += forward;
        if (in.s) vel -= forward;
        if (in.a) vel -= right;
        if (in.d) vel += right;
        vel *= speed;
        p.pos.x += vel.x * dt;
        p.pos.z += vel.z * dt;
        p.airVX  = vel.x;          // remember in case we leave the ground this tick
        p.airVZ  = vel.z;
    } else {
        p.pos.x += p.airVX * dt;
        p.pos.z += p.airVZ * dt;
    }

    // vertical: jump only when grounded, then integrate gravity
    if (in.jump && grounded) p.velY = JUMP_SPEED;
    p.velY  -= GRAVITY * dt;
    p.pos.y  = fromY + p.velY * dt;

    // land first (so a box top you reach isn't mistaken for a wall), then push
    // out of anything taller you're beside
    float floor = supportHeight(p, fromY);
    if (p.pos.y <= floor) {
        p.pos.y = floor;
        p.velY  = 0.0f;
    }
    collideXZ(p);
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
                target.deaths++;
                if (b.ownerID >= 0) gs.players[b.ownerID].kills++;
            }
            break;
        }
        if (b.active) active++;
    }
    gs.bulletCount = active;
}
