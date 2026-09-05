#include "physics.h"
#include "map.h"
#include "tree_scatter.h"
#include "playerpose.h"
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

int playerHitRegions(const glm::vec3& pos, bool crouched, float yaw, float pitch,
                     float lean, bool ads, uint8_t weaponId,
                     HitRegion out[MAX_HIT_REGIONS]) {
    // The hitboxes ARE the rendered player: buildPlayerPose emits the same oriented
    // boxes drawPlayerSkeleton draws (FK spine, IK arms onto the weapon grips, the
    // weapon, IK legs), here posed at rest stride (phase = amp = 0 — the walk cycle is
    // client-side animation the server can't reproduce). Each part maps to a damage
    // multiplier; ~1 cm pad everywhere, legs a bit wider to cover the stride swing.
    PoseBox boxes[MAX_POSE_BOXES];
    int n = buildPlayerPose(pos, yaw, pitch, lean, weaponId, crouched,
                            ads ? 1.0f : 0.0f, 0.0f, 0.0f, boxes);
    for (int i = 0; i < n; i++) {
        float     mult = 1.0f;
        glm::vec3 pad{0.01f};
        switch (boxes[i].part) {
            case POSE_HEAD: case POSE_NOSE: mult = 2.0f; break;
            case POSE_NECK:                 mult = 1.5f; pad = glm::vec3(0.02f); break;
            case POSE_LEG:  case POSE_FOOT: mult = 0.8f; pad = {0.04f, 0.01f, 0.04f}; break;
            default: break;   // pelvis/torso/arms/hands/gun: 1.0x
        }
        out[i] = {boxes[i].M, glm::inverse(boxes[i].M), boxes[i].half + pad, mult};
    }
    return n;
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
    bool  airborne = p.pos.y > terrainHeight(p.pos.x, p.pos.z) + 0.05f;
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
    float floor = terrainHeight(p.pos.x, p.pos.z);
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
        float clearance = (p.crouched ? CROUCH_HEIGHT : STAND_HEIGHT) + 0.1f;
        if (b.center.y - b.half.y > p.pos.y + clearance) continue;
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

// Push the footprint out of tree trunks: radial cylinder resolve, same authority as
// box collision (runs on server + client prediction). Trees are point+radius, so the
// nearest surface is always radially outward — no least-overlap axis choice needed.
static constexpr float TREE_QUERY_R = FOOT_R + 0.7f;  // covers the fattest trunk flare
static void collideTrees(Player& p) {
    gTreeColGrid.forEachNear(p.pos.x, p.pos.z, TREE_QUERY_R, [&](int i) {
        const TreeCol& t = gTreeCols[i];
        float dx = p.pos.x - t.x, dz = p.pos.z - t.z;
        float rr = FOOT_R + t.r;
        float d2 = dx * dx + dz * dz;
        if (d2 >= rr * rr) return;
        if (d2 < 1e-6f) { p.pos.x += rr; return; }   // dead-centre: shove out on X
        float d = sqrtf(d2), push = rr - d;
        p.pos.x += dx / d * push;
        p.pos.z += dz / d * push;
    });
}

void movePlayer(Player& p, const InputState& in, float dt) {
    glm::vec3 forward = glm::normalize(glm::vec3(
        cos(glm::radians(in.yaw)), 0, sin(glm::radians(in.yaw))));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    p.yaw = in.yaw;
    p.crouched = in.crouch;

    glm::vec2 prevXZ(p.pos.x, p.pos.z);   // for the wade-limit revert below
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
    collideTrees(p);

    // Wade limit: the sea (terrain below SEA_LEVEL) is walkable to waist depth, then
    // acts as a wall. Resolve per axis so the shoreline slides like box collision.
    constexpr float WADE_DEPTH = 1.1f;
    if (terrainHeight(p.pos.x, p.pos.z) < SEA_LEVEL - WADE_DEPTH) {
        glm::vec3 tryX(prevXZ.x, 0, p.pos.z);   // keep the axis that stays shallow
        glm::vec3 tryZ(p.pos.x, 0, prevXZ.y);
        if      (terrainHeight(tryZ.x, tryZ.z) >= SEA_LEVEL - WADE_DEPTH) { p.pos.z = prevXZ.y; }
        else if (terrainHeight(tryX.x, tryX.z) >= SEA_LEVEL - WADE_DEPTH) { p.pos.x = prevXZ.x; }
        else { p.pos.x = prevXZ.x; p.pos.z = prevXZ.y; }
        p.pos.y = fmaxf(p.pos.y, terrainHeight(p.pos.x, p.pos.z));
    }
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
