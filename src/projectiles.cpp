#include "physics.h"
#include "map.h"
#include "tree_collision.h"
#include <cmath>

// Sweep a segment against one oriented hit region: map both endpoints into the box's
// local frame (affine, so the entry fraction t is preserved) and run the AABB slab test.
static bool segmentRegion(const glm::vec3& p0, const glm::vec3& p1,
                          const HitRegion& rg, float& tHit) {
    glm::vec3 q0 = glm::vec3(rg.invM * glm::vec4(p0, 1.0f));
    glm::vec3 q1 = glm::vec3(rg.invM * glm::vec4(p1, 1.0f));
    return segmentAabb(q0, q1, glm::vec3(0.0f), rg.half, tHit);
}

// Segment p0->p1 vs the ground. Flat y=0 plane on arena maps; on heightfield maps
// it marches the terrain (the bullet can step several metres per tick, so sample
// along the segment and lerp at the first point that dips to/under the terrain).
static bool segmentGround(const glm::vec3& p0, const glm::vec3& p1, float& tHit) {
    if (gTerrainMode == TERRAIN_OFF) {
        if (p1.y > 0.0f) return false;            // stays above ground this tick
        if (p0.y <= 0.0f) { tHit = 0.0f; return true; }
        tHit = p0.y / (p0.y - p1.y);
        return true;
    }
    const int N = 8;
    float prevDiff = p0.y - terrainHeight(p0.x, p0.z);
    if (prevDiff <= 0.0f) { tHit = 0.0f; return true; }   // already under terrain
    for (int i = 1; i <= N; i++) {
        float t = (float)i / N;
        glm::vec3 s = p0 + (p1 - p0) * t;
        float diff = s.y - terrainHeight(s.x, s.z);
        if (diff <= 0.0f) {                                // crossed the surface
            float frac = prevDiff / (prevDiff - diff);     // lerp within this step
            tHit = (float)(i - 1) / N + frac / N;
            return true;
        }
        prevDiff = diff;
    }
    return false;
}

// Distance-based damage scale: full up to falloffStart, lerps to falloffMin by end.
static float dmgScale(const WeaponDef& w, float dist) {
    if (dist <= w.falloffStart) return 1.0f;
    if (dist >= w.falloffEnd)   return w.falloffMin;
    float u = (dist - w.falloffStart) / (w.falloffEnd - w.falloffStart);
    return 1.0f + (w.falloffMin - 1.0f) * u;
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

void updateBullets(GameState& gs, float dt, RewindLookup lookup, const void* ctx,
                   Impact* outImpacts, int* outCount, int maxImpacts) {
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

        // Nearest surface along p0->p1. World (box/ground/tree) blocks; a player is a hit.
        // hitBox tracks the nearest WORLD surface so we can stamp an oriented decal:
        // >=0 = that map box index, -1 = ground, -2 = none/player, -3 = tree.
        float bestT   = 2.0f;
        int   hitPid  = -1;
        int   hitBox  = -2;
        float hitMult = 1.0f;       // damage multiplier of the nearest player region
        glm::vec3 treeNormal(0, 1, 0);
        float t;
        for (int m = 0; m < gMapBoxCount; m++)
            if (segmentAabb(p0, p1, gMapBoxes[m].center, gMapBoxes[m].half, t) && t < bestT) {
                bestT = t; hitPid = -1; hitBox = m;
            }
        if (segmentGround(p0, p1, t) && t < bestT) { bestT = t; hitPid = -1; hitBox = -1; }

        if (sweepTreeTrunks(p0, p1, t, treeNormal) && t < bestT) {
            bestT = t; hitBox = -3;
        }

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
            float     hitYaw      = target.yaw;
            float     hitPitch    = target.pitch;
            float     hitLean     = target.lean;
            bool      hitAds      = target.ads;
            uint8_t   hitWeapon   = target.weaponId;
            if (lookup) {
                bool wasAlive = false;
                if (!lookup(ctx, pid, b.compRewind, hitPos, hitCrouched, hitYaw,
                            hitPitch, hitLean, hitAds, hitWeapon, wasAlive))
                    continue;
                if (!wasAlive) continue;
            }
            // Sweep each body region; nearest one wins and carries its multiplier.
            HitRegion rg[MAX_HIT_REGIONS];
            int nr = playerHitRegions(hitPos, hitCrouched, hitYaw, hitPitch, hitLean,
                                      hitAds, hitWeapon, rg);
            for (int k = 0; k < nr; k++)
                if (segmentRegion(p0, p1, rg[k], t) && t < bestT) {
                    bestT = t; hitPid = pid; hitMult = rg[k].mult;
                }
        }

        if (bestT <= 1.0f) {                        // something stopped the bullet
            glm::vec3 impact = p0 + (p1 - p0) * bestT;
            b.pos    = impact;
            b.active = false;
            if (hitPid < 0 && outImpacts && outCount && *outCount < maxImpacts) {
                // World surface: record the impact + outward normal for a decal. Box
                // normal = the face nearest the impact point; ground = straight up.
                glm::vec3 n = hitBox == -3 ? treeNormal : glm::vec3(0, 1, 0);
                if (hitBox >= 0) {
                    glm::vec3 d = impact - gMapBoxes[hitBox].center;
                    glm::vec3 h = gMapBoxes[hitBox].half;
                    int   ax = 0; float gap = fabsf(h.x - fabsf(d.x));
                    float gy = fabsf(h.y - fabsf(d.y)); if (gy < gap) { gap = gy; ax = 1; }
                    float gz = fabsf(h.z - fabsf(d.z)); if (gz < gap) {        ax = 2; }
                    n = glm::vec3(0); n[ax] = d[ax] >= 0.0f ? 1.0f : -1.0f;
                }
                outImpacts[*outCount] = {impact, n};
                (*outCount)++;
            }
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
