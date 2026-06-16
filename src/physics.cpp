#include "physics.h"
#include "map.h"
#include <cmath>
#include <cstdlib>

bool aabbHit(glm::vec3 p, glm::vec3 playerPos, bool crouched) {
    float h = (crouched ? CROUCH_HEIGHT : STAND_HEIGHT) * 0.5f;  // half body height
    glm::vec3 center = playerPos + glm::vec3(0, h, 0);
    glm::vec3 half   = {0.4f, h, 0.4f};
    return fabsf(p.x - center.x) < half.x &&
           fabsf(p.y - center.y) < half.y &&
           fabsf(p.z - center.z) < half.z;
}

// Slab method: segment p0->p1 vs AABB. tHit = entry fraction in [0,1]. A segment
// that starts inside the box returns tHit = 0 (immediate hit).
bool segmentAabb(const glm::vec3& p0, const glm::vec3& p1,
                 const glm::vec3& center, const glm::vec3& half, float& tHit) {
    glm::vec3 d = p1 - p0;
    float tmin = 0.0f, tmax = 1.0f;
    for (int a = 0; a < 3; a++) {
        float lo = center[a] - half[a];
        float hi = center[a] + half[a];
        if (fabsf(d[a]) < 1e-8f) {
            if (p0[a] < lo || p0[a] > hi) return false;  // parallel, outside the slab
        } else {
            float inv = 1.0f / d[a];
            float t1 = (lo - p0[a]) * inv;
            float t2 = (hi - p0[a]) * inv;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    tHit = tmin;
    return true;
}

// Segment vs the y=0 ground plane. tHit = fraction where it crosses, if it does.
static bool segmentGround(const glm::vec3& p0, const glm::vec3& p1, float& tHit) {
    if (p1.y > 0.0f) return false;            // stays above ground this tick
    if (p0.y <= 0.0f) { tHit = 0.0f; return true; }
    tHit = p0.y / (p0.y - p1.y);
    return true;
}

// Distance-based damage scale: full up to falloffStart, lerps to falloffMin by end.
static float dmgScale(const WeaponDef& w, float dist) {
    if (dist <= w.falloffStart) return 1.0f;
    if (dist >= w.falloffEnd)   return w.falloffMin;
    float u = (dist - w.falloffStart) / (w.falloffEnd - w.falloffStart);
    return 1.0f + (w.falloffMin - 1.0f) * u;
}

int playerHitRegions(const glm::vec3& pos, bool crouched, HitRegion out[3]) {
    // Heights authored for STAND_HEIGHT (2.0); scaled down when crouched so the
    // stack always fits the body. Boxes are centered on the player's vertical axis
    // (yaw-independent) so they line up with the lag-comp rewind (pos + crouched).
    float s = crouched ? (CROUCH_HEIGHT / STAND_HEIGHT) : 1.0f;
    // XZ footprints are square so they cover the arms (which rotate with yaw, not
    // stored in lag comp) — the body is a solid blocker, no shooting "between" the
    // arms and torso to tag someone behind.
    out[0] = {pos + glm::vec3(0, 0.50f * s, 0), {0.32f, 0.50f * s, 0.32f}, 0.8f}; // legs (covers stride)
    out[1] = {pos + glm::vec3(0, 1.30f * s, 0), {0.28f, 0.35f * s, 0.28f}, 1.0f}; // torso+arms
    out[2] = {pos + glm::vec3(0, 1.80f * s, 0), {0.17f, 0.16f * s, 0.17f}, 2.0f}; // head
    return 3;
}

glm::vec3 dirFromYawPitch(float yaw, float pitch) {
    return glm::normalize(glm::vec3(
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch))
    ));
}

static float randf() { return (float)rand() / (float)RAND_MAX; }

// Perturb `dir` within a cone of half-angle `deg`. Uniform over the disc so
// shots cluster toward the center, fall off to the edge.
static glm::vec3 applySpread(glm::vec3 dir, float deg) {
    if (deg <= 0.0f) return dir;
    glm::vec3 ref   = fabsf(dir.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 right = glm::normalize(glm::cross(dir, ref));
    glm::vec3 up    = glm::cross(right, dir);
    float r   = tanf(glm::radians(deg)) * sqrtf(randf());
    float phi = randf() * 6.2831853f;
    return glm::normalize(dir + right * (r * cosf(phi)) + up * (r * sinf(phi)));
}

float aimSpread(const Player& p, const InputState& in) {
    float base    = in.ads ? ADS_SPREAD_DEG : HIP_SPREAD_DEG;
    bool  airborne = p.pos.y > 0.05f;
    bool  moving   = in.w || in.a || in.s || in.d;
    float move     = airborne ? JUMP_SPREAD_DEG
                   : (in.sprint && moving) ? SPRINT_SPREAD_DEG
                   : moving ? MOVE_SPREAD_DEG : 0.0f;
    if (in.crouch && !moving && !airborne) base *= CROUCH_SPREAD_MULT;
    if (in.ads) move *= ADS_MOVE_MULT;
    return base + move;
}

void weaponShot(float yaw, float pitch, const glm::vec3& eye, bool ads,
                float spreadDeg, glm::vec3& outOrigin, glm::vec3& outDir) {
    glm::vec3 front = dirFromYawPitch(yaw, pitch);
    // The muzzle sits on the crosshair ray and fires straight along it, so shots are
    // dead-center at every range (point-blank included), minus bullet drop. A small
    // forward offset for hipfire just makes the tracer read as leaving the barrel.
    outOrigin = eye + front * (ads ? 0.0f : 0.25f);
    outDir    = applySpread(front, spreadDeg);
}


static constexpr float FOOT_R = 0.4f;  // player XZ footprint half-extent

// Highest surface the player can stand on under their footprint: arena floor (0)
// or a box top they are coming down onto. `fromY` is the height before this tick's
// fall — a box counts only if the player was at/above its top, so you land when
// crossing a top from above but don't snap onto one you're rising past.
static float supportHeight(const Player& p, float fromY) {
    float floor = 0.0f;
    for (int i = 0; i < gMapBoxCount; i++) {
        const Box& b = gMapBoxes[i];
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
    for (int i = 0; i < gMapBoxCount; i++) {
        const Box& b = gMapBoxes[i];
        float top = b.center.y + b.half.y;
        if (p.pos.y >= top - 0.05f) continue;  // on/above the box → no side push
        float dx = (b.half.x + FOOT_R) - fabsf(p.pos.x - b.center.x);
        float dz = (b.half.z + FOOT_R) - fabsf(p.pos.z - b.center.z);
        if (dx <= 0.0f || dz <= 0.0f) continue;
        if (dx < dz) p.pos.x += (p.pos.x < b.center.x) ? -dx : dx;
        else         p.pos.z += (p.pos.z < b.center.z) ? -dz : dz;
    }
    if (p.pos.x >  gArenaHalf) p.pos.x =  gArenaHalf;
    if (p.pos.x < -gArenaHalf) p.pos.x = -gArenaHalf;
    if (p.pos.z >  gArenaHalf) p.pos.z =  gArenaHalf;
    if (p.pos.z < -gArenaHalf) p.pos.z = -gArenaHalf;
}

void movePlayer(Player& p, const InputState& in, float dt) {
    glm::vec3 forward = glm::normalize(glm::vec3(
        cos(glm::radians(in.yaw)), 0, sin(glm::radians(in.yaw))));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    p.yaw = in.yaw;
    p.crouched = in.crouch;

    float fromY    = p.pos.y;
    bool  grounded = fromY <= supportHeight(p, fromY) + 0.001f;

    // horizontal: full control on the ground; in the air keep takeoff momentum
    if (grounded) {
        float speed = in.crouch ? CROUCH_SPEED : in.sprint ? SPRINT_SPEED : MOVE_SPEED;
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

bool spawnBullet(GameState& gs, const glm::vec3& eyePos, const glm::vec3& dir, int ownerID,
                 float compRewind) {
    Player& owner = gs.players[ownerID];
    if (!owner.alive || owner.mag <= 0 || owner.reloading) return false;
    const WeaponDef& wd = weaponDef(owner.weaponId);
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet& b = gs.bullets[i];
        if (b.active) continue;
        b.pos        = eyePos;
        b.origin     = eyePos;
        b.vel        = dir * wd.muzzleVel;
        b.lifetime   = wd.ttl;
        b.ownerID    = ownerID;
        b.weaponId   = owner.weaponId;
        b.active     = true;
        b.compRewind = compRewind;
        owner.mag--;
        return true;
    }
    return false;
}

// Advance a reload: start one if requested and possible, finish when the timer
// elapses by topping the magazine from reserve. Auto-starts when the mag runs
// dry (no R needed). Authoritative; run per tick.
void updateReload(Player& p, bool wantReload, float dt) {
    const WeaponDef& wd = weaponDef(p.weaponId);
    bool autoEmpty = p.mag <= 0;            // dry mag → reload without pressing R
    if (p.reloadTimer > 0.0f) {
        p.reloadTimer -= dt;
        if (p.reloadTimer <= 0.0f) {
            p.reloadTimer = 0.0f;
            int need = wd.magSize - p.mag;
            int take = need < p.reserve ? need : p.reserve;
            p.mag     += take;
            p.reserve -= take;
        }
    } else if ((wantReload || autoEmpty) && p.mag < wd.magSize && p.reserve > 0) {
        p.reloadTimer = wd.reloadTime;
    }
    p.reloading = p.reloadTimer > 0.0f;
}

void updateBullets(GameState& gs, float dt, RewindLookup lookup, const void* ctx) {
    int active = 0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet& b = gs.bullets[i];
        if (!b.active) continue;

        // Integrate velocity (gravity + quadratic air drag), then sweep the segment
        // the bullet travels this tick against the world — velocity-independent, so
        // fast rounds can't tunnel through cover or players.
        const WeaponDef& wd = weaponDef(b.weaponId);
        b.vel.y -= GRAVITY * dt;
        if (wd.dragK > 0.0f) {
            float speed = glm::length(b.vel);
            if (speed > 0.0f) b.vel -= (wd.dragK * speed * dt) * b.vel;
        }
        glm::vec3 p0 = b.pos;
        glm::vec3 p1 = p0 + b.vel * dt;
        b.lifetime -= dt;

        // Nearest surface along p0->p1. World (box/ground) blocks; a player is a hit.
        float bestT   = 2.0f;
        int   hitPid  = -1;
        float hitMult = 1.0f;       // damage multiplier of the nearest player region
        float t;
        for (int m = 0; m < gMapBoxCount; m++)
            if (segmentAabb(p0, p1, gMapBoxes[m].center, gMapBoxes[m].half, t) && t < bestT) {
                bestT = t; hitPid = -1;
            }
        if (segmentGround(p0, p1, t) && t < bestT) { bestT = t; hitPid = -1; }

        for (int pid = 0; pid < MAX_PLAYERS; pid++) {
            if (pid == b.ownerID) continue;
            if (!(gs.usedMask & (1u << pid))) continue;
            Player& target = gs.players[pid];
            if (!target.alive) continue;

            // Test against the rewound hitbox (lag comp) when a history lookup is
            // provided, else against the current position. Damage/kills always
            // apply to the current authoritative player.
            glm::vec3 hitPos      = target.pos;
            bool      hitCrouched = target.crouched;
            if (lookup) {
                bool wasAlive = false;
                if (!lookup(ctx, pid, b.compRewind, hitPos, hitCrouched, wasAlive)) continue;
                if (!wasAlive) continue;
            }
            // Sweep each body region; nearest one wins and carries its multiplier.
            HitRegion rg[3];
            int nr = playerHitRegions(hitPos, hitCrouched, rg);
            for (int k = 0; k < nr; k++)
                if (segmentAabb(p0, p1, rg[k].center, rg[k].half, t) && t < bestT) {
                    bestT = t; hitPid = pid; hitMult = rg[k].mult;
                }
        }

        if (bestT <= 1.0f) {                        // something stopped the bullet
            glm::vec3 impact = p0 + (p1 - p0) * bestT;
            b.pos    = impact;
            b.active = false;
            if (hitPid >= 0) {                      // closest surface was a player → hit
                Player& target = gs.players[hitPid];
                float dist = glm::length(impact - b.origin);
                int   dmg  = (int)(wd.dmg * dmgScale(wd, dist) * hitMult);
                if (dmg < 1) dmg = 1;
                target.hp -= dmg;
                if (b.ownerID >= 0) {               // record for the shooter's hit marker
                    Player& shooter = gs.players[b.ownerID];
                    shooter.hits++;
                    shooter.lastHitPos = impact;
                }
                if (target.hp <= 0) {
                    target.hp           = 0;
                    target.alive        = false;
                    target.respawnTimer = RESPAWN_TIME;
                    target.deaths++;
                    if (b.ownerID >= 0) gs.players[b.ownerID].kills++;
                }
            }
            continue;
        }

        b.pos = p1;                                 // free flight this tick
        if (b.lifetime <= 0.0f) { b.active = false; continue; }
        active++;
    }
    gs.bulletCount = active;
}
