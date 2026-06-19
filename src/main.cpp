// Client: starts in training mode (offline) by default. Auto-connects only with
// `./game <ip>`. Press C for an in-game IP prompt (127.0.0.1 pre-filled).
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include "platform.h"
#include "renderer.h"
#include "camera.h"
#include "input.h"
#include "physics.h"
#include "network.h"
#include "map.h"
#include "hud.h"
#include "audio.h"
#include "material.h"
#include "connect_prompt.h"
#include "skeleton.h"
#include "weapon_visual.h"

static const glm::vec3 COLOR_ENEMY        = {0.80f, 0.30f, 0.20f};
static const glm::vec3 COLOR_BULLET_OWN   = {1.00f, 0.90f, 0.20f};
static const glm::vec3 COLOR_BULLET_ENEMY = {1.00f, 0.40f, 0.10f};
static const glm::vec3 COLOR_SELF         = {0.30f, 0.55f, 0.85f};  // own avatar in mirror

// Persistent bullet-impact decal: a small patch laid flat on whatever surface the round
// struck (ground, cover, walls). Stamped from the local sim offline and from the
// server's PKT_IMPACT online, so every world detail shows hits, not just the range wall.
struct Decal { glm::vec3 pos; glm::vec3 normal; };
static const glm::vec3 COLOR_DECAL = {1.0f, 0.85f, 0.20f};   // matches the old range-wall marks
// Axis-aligned normals, indexed by the protocol's ImpactDir code (IMP_PX..IMP_NZ).
static const glm::vec3 IMPACT_DIRS[6] = {
    {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1},
};

// Offline shooting range: a large flat target wall (client-only, not in MAP_BOXES
// so it never affects online play) that the practice spawn faces. Bullets that
// reach it leave persistent marks so the spread/recoil pattern and drop are visible.
static const glm::vec3 RANGE_WALL_CENTER = {26.0f, 4.0f, 0.0f};
static const glm::vec3 RANGE_WALL_HALF   = {0.6f, 4.0f, 13.0f};
static const float     RANGE_WALL_FACE   = 25.4f;  // front face = center.x - half.x
static const float     RANGE_BULLSEYE_Y  = 1.7f;   // matches standing eye height
constexpr int          DECAL_MAX         = 512;    // ring buffer of impact decals

// First-person weapon state (client cosmetic; gameplay is server-authoritative).
struct ViewModel {
    float adsT       = 0.0f;  // 0 = hipfire pose, 1 = aimed
    float flashTimer = 0.0f;  // muzzle flash seconds remaining
    float recoilT    = 0.0f;  // recoil kick, 1 on fire, decays to 0
};

// Draws the held gun from a few oriented cubes, anchored to the camera and
// lerped between hipfire (lower-right) and ADS (centered under crosshair).
static void drawViewModel(Renderer& r, const Camera& cam, const ViewModel& vm) {
    glm::vec3 front = cam.front();
    glm::vec3 up    = cam.up();                                   // rolled by lean
    glm::vec3 right = glm::normalize(glm::cross(front, up));

    glm::vec3 hipOff = right * 0.17f - up * 0.30f + front * 0.35f;
    // Uzi: align hollow sight center (local y=0.06, z=-0.02) with the aim ray.
    glm::vec3 adsOff = (gWeaponId == WEP_UZI)
        ? (-up * 0.06f + front * 0.32f)
        : (-up * 0.05f + front * 0.30f);
    glm::vec3 off    = glm::mix(hipOff, adsOff, vm.adsT);
    glm::vec3 anchor = cam.eyePos() + off - front * (vm.recoilT * 0.08f);

    glm::mat4 basis(glm::vec4(right, 0), glm::vec4(up, 0),
                    glm::vec4(front, 0), glm::vec4(0, 0, 0, 1));
    glm::mat4 anchorM = glm::translate(glm::mat4(1.0f), anchor) * basis;
    auto part = [&](glm::vec3 lp, glm::vec3 sz, glm::vec3 col) {
        glm::mat4 m = glm::scale(glm::translate(anchorM, lp), sz);
        r.drawCubeModel(m, col);
    };
    const glm::vec3 metal = {0.12f, 0.12f, 0.14f};
    const glm::vec3 dark  = {0.20f, 0.20f, 0.23f};
    const glm::vec3 poly  = {0.08f, 0.08f, 0.09f};  // pistol polymer frame

    if (gWeaponId == WEP_GLOCK19) {                 // compact pistol
        part({0.0f,  0.02f,  0.06f}, {0.050f, 0.060f, 0.20f}, metal); // slide
        part({0.0f, -0.03f,  0.03f}, {0.045f, 0.050f, 0.15f}, poly);  // frame
        part({0.0f,  0.02f,  0.17f}, {0.026f, 0.026f, 0.05f}, dark);  // barrel tip
        part({0.0f, -0.12f, -0.05f}, {0.050f, 0.130f, 0.06f}, poly);  // grip
        part({0.0f, -0.18f, -0.05f}, {0.046f, 0.040f, 0.05f}, metal); // mag base
    } else {                                        // Uzi — boxy SMG
        part({0.0f,  0.00f,  0.01f}, {0.078f, 0.10f,  0.26f}, metal); // receiver
        // Red-dot sight — hollow housing (see-through window along barrel axis).
        {
            const glm::vec3 sc = {0.0f, 0.06f, -0.02f};
            const float wx = 0.050f, wy = 0.040f, wz = 0.12f, wt = 0.009f;
            part({sc.x, sc.y + wy * 0.5f - wt * 0.5f, sc.z}, {wx, wt, wz}, dark); // top
            part({sc.x, sc.y - wy * 0.5f + wt * 0.5f, sc.z}, {wx, wt, wz}, dark); // bottom
            part({sc.x - wx * 0.5f + wt * 0.5f, sc.y, sc.z}, {wt, wy - wt * 2.0f, wz}, dark); // left
            part({sc.x + wx * 0.5f - wt * 0.5f, sc.y, sc.z}, {wt, wy - wt * 2.0f, wz}, dark); // right
        }
        part({0.0f,  0.02f,  0.22f}, {0.032f, 0.032f, 0.16f}, dark);  // barrel
        part({0.0f, -0.11f, -0.02f}, {0.050f, 0.140f, 0.07f}, metal); // grip
        part({0.0f, -0.20f, -0.02f}, {0.044f, 0.190f, 0.05f}, dark);  // long magazine
    }

    // Grip + muzzle come from the shared per-weapon anchor table (weapon_visual.h), so
    // every gun's hand placement and barrel length live in one place.
    const WeaponVisual& wv = weaponVisual(gWeaponId);
    const float gripY = wv.fpFireGrip.y, gripZ = wv.fpFireGrip.z;

    // Right (firing) hand only — PUBG-style, minimal screen space. A fist wrapping
    // the grip plus a short forearm angled down-back so it leaves frame fast; the gun
    // sits in front and occludes most of it. ADS/recoil come free from anchor space.
    const glm::vec3 glove = {0.46f, 0.35f, 0.28f};   // tan fingerless glove
    glm::vec3 fistC = {wv.fpFireGrip.x, gripY, gripZ};  // fist centered on the grip
    r.drawCubeModel(glm::scale(glm::translate(anchorM, fistC),
                               glm::vec3(0.085f, 0.095f, 0.105f)), glove);
    // Forearm: pivot at the wrist (just under the fist), tilt back about the right
    // axis so the limb runs from the lower-right toward the shoulder, then hang it.
    glm::mat4 fore = glm::translate(anchorM, glm::vec3(0.02f, gripY - 0.05f, gripZ - 0.01f))
                   * glm::rotate(glm::mat4(1.0f), glm::radians(24.0f), glm::vec3(1, 0, 0))
                   * glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0, 0, 1))
                   * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.11f, 0.0f));
    r.drawCubeModel(glm::scale(fore, glm::vec3(0.072f, 0.22f, 0.082f)), glove);

    if (vm.flashTimer > 0.0f) {                                    // muzzle flash
        glm::vec3 muzzle = anchor + up * wv.fpMuzzle.y + front * wv.fpMuzzle.z;
        r.drawCube(muzzle, glm::vec3(0.16f), {1.0f, 0.85f, 0.35f});
    }
}

// 2-bone IK: given root joint S and end target T (world space) and bone lengths L1/L2,
// return the middle joint position. The joint bulges toward `bendHint` (the perpendicular
// component of it) — pass world-down for elbows, the body's forward for knees. The limb
// is then drawn as two boxes S->mid->T, so it stays correct at any yaw/pitch/lean.
static glm::vec3 ikJoint(glm::vec3 S, glm::vec3 T, float L1, float L2, glm::vec3 bendHint) {
    glm::vec3 v = T - S;
    float d = glm::length(v);
    float dmax = L1 + L2 - 1e-3f, dmin = fabsf(L1 - L2) + 1e-3f;
    if (d > dmax)            { v *= dmax / d; d = dmax; }
    else if (d < dmin && d > 1e-5f) { v *= dmin / d; d = dmin; }
    glm::vec3 dir = (d > 1e-5f) ? v / d : glm::vec3(0, 0, 1);
    float cosA = (L1 * L1 + d * d - L2 * L2) / (2.0f * L1 * d);
    cosA = cosA < -1.0f ? -1.0f : cosA > 1.0f ? 1.0f : cosA;
    float proj = L1 * cosA, perp = L1 * sqrtf(1.0f - cosA * cosA);
    glm::vec3 bend = bendHint - dir * glm::dot(bendHint, dir);   // perpendicular to dir
    bend = (glm::length(bend) > 1e-4f) ? glm::normalize(bend) : glm::vec3(0, 0, 1);
    return S + dir * proj + bend * perp;
}

// Box spanning world points A..B along its local +Y, square cross-section `w` wide.
// For IK limbs whose endpoints are solved rather than swung from a fixed joint.
static void drawSegment(Renderer& r, glm::vec3 A, glm::vec3 B, float w, const glm::vec3& c) {
    glm::vec3 mid = (A + B) * 0.5f, dir = B - A;
    float len = glm::length(dir);
    if (len < 1e-5f) return;
    dir /= len;
    glm::vec3 up = fabsf(dir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    glm::vec3 bx = glm::normalize(glm::cross(up, dir));
    glm::vec3 bz = glm::cross(dir, bx);
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(bx * w, 0.0f);
    m[1] = glm::vec4(dir * len, 0.0f);
    m[2] = glm::vec4(bz * w, 0.0f);
    m[3] = glm::vec4(mid, 1.0f);
    r.drawCubeModel(m, c);
}

// A flat impact patch oriented to the surface normal: a thin box (normal axis short,
// the other two ~decal-sized) nudged just off the surface so it reads as a mark.
static void drawDecal(Renderer& r, const Decal& d) {
    glm::vec3 n  = d.normal;
    glm::vec3 t  = fabsf(n.y) > 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    glm::vec3 bx = glm::normalize(glm::cross(t, n));
    glm::vec3 bz = glm::normalize(glm::cross(n, bx));
    // Columns ordered (bz, n, bx) so the basis is right-handed (det +1): with the
    // left-handed order the outward broad face winds backwards and back-face culling
    // drops it, leaving only the thin rim visible. n is the short (thickness) axis.
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(bz * 0.14f, 0.0f);
    m[1] = glm::vec4(n  * 0.02f, 0.0f);
    m[2] = glm::vec4(bx * 0.14f, 0.0f);
    m[3] = glm::vec4(d.pos + n * 0.02f, 1.0f);    // sit proud of the surface (no z-fight)
    r.drawCubeModel(m, COLOR_DECAL);
}

// Renders a player from the bone-array Skeleton (skeleton.h) — the ragdoll-ready path.
// The spine (pelvis/torso/neck/head/nose) is drawn from skeleton FK; arms and legs are
// solved by two-bone IK (ikJoint) each frame, with
// the skeleton's own limb-joint positions (shoulders = upper-arm joints, hips = thigh
// joints) used as the IK anchors. Aim pitch tilts the head + gun, lean rolls the upper
// body, crouch squashes vertically. The weapon sits at the chest with both hands IK'd
// onto its grips; feet plant on the terrain with a speed-scaled walk stride. Forward is
// +Z after the root yaw, matching the nose. One shared skeleton, re-posed per call.
static void drawPlayerSkeleton(Renderer& r, const glm::vec3& pos, float yaw, float pitch,
                               float lean, uint8_t weaponId, bool crouched, float ads,
                               float phase, float amp, const glm::vec3& bodyCol) {
    static Skeleton skel = makeBoxMan();
    const glm::vec3 limb     = bodyCol * 0.85f;
    const glm::vec3 skin     = {0.90f, 0.78f, 0.62f};
    const glm::vec3 handCol  = {0.30f, 0.30f, 0.33f};
    const glm::vec3 gunMetal = {0.12f, 0.12f, 0.14f};
    const glm::vec3 gunDark  = {0.20f, 0.20f, 0.23f};
    skel.bones[BONE_PELVIS].color = skel.bones[BONE_TORSO].color =
        skel.bones[BONE_NECK].color = bodyCol;
    skel.bones[BONE_HEAD].color = skin;
    skel.bones[BONE_NOSE].color = {0.90f, 0.30f, 0.20f};

    // Spine pose: lean roll on the torso, a little aim tilt on the neck. Limb-bone
    // localRots stay at rest (identity) — the limbs are drawn by IK, not FK, below; we
    // only read their joint positions as anchors.
    float la = lean * glm::radians(LEAN_ANGLE_DEG);
    skel.bones[BONE_TORSO].localRot = glm::angleAxis(-la, glm::vec3(0, 0, 1));
    skel.bones[BONE_NECK].localRot  = glm::angleAxis(glm::radians(-pitch) * 0.5f, glm::vec3(1, 0, 0));

    float s = crouched ? (CROUCH_HEIGHT / STAND_HEIGHT) : 1.0f;   // vertical squash about feet
    glm::mat4 root = glm::translate(glm::mat4(1.0f), pos)
                   * glm::rotate(glm::mat4(1.0f), 1.5707963f - glm::radians(yaw), glm::vec3(0, 1, 0))
                   * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, s, 1.0f))
                   * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.95f, 0));
    skel.computeWorld(root);

    // Spine + head + nose from FK (skip the 8 limb bones — IK draws those).
    const int spine[5] = {BONE_PELVIS, BONE_TORSO, BONE_NECK, BONE_HEAD, BONE_NOSE};
    for (int b : spine) {
        glm::mat4 m = skel.world[b]
                    * glm::translate(glm::mat4(1.0f), skel.bones[b].boxOffset)
                    * glm::scale(glm::mat4(1.0f), skel.bones[b].halfExtents * 2.0f);
        r.drawCubeModel(m, skel.bones[b].color);
    }

    // --- Weapon at the chest (pitched by aim), hung off the torso frame so it leans and
    // crouches with the body; both hands IK'd onto its grips. ---
    const float upArm = 0.30f, foreArm = 0.30f;
    // Hold pose lerps from the hipfire chest mount to a raised ADS pose: the gun rises
    // to the eyeline and onto the body centerline, and the walk bob fades as you aim so
    // it reads as steady. Aim pitch still tilts the whole weapon; the hands IK onto the
    // grips below, so they follow the raised gun for free.
    float bob = sinf(phase) * amp * 0.03f * (1.0f - ads);
    glm::vec3 hipT = {0.05f, 0.41f + bob, 0.20f};
    glm::vec3 adsT = {0.0f, 0.54f, 0.21f};
    glm::mat4 hold = skel.world[BONE_TORSO]
        * glm::translate(glm::mat4(1.0f), glm::mix(hipT, adsT, ads))
        * glm::rotate(glm::mat4(1.0f), -glm::radians(pitch), glm::vec3(1, 0, 0));
    auto gun = [&](glm::vec3 lp, glm::vec3 sz, const glm::vec3& c) {
        r.drawCubeModel(hold * glm::translate(glm::mat4(1.0f), lp)
                             * glm::scale(glm::mat4(1.0f), sz), c);
    };
    if (weaponId == WEP_GLOCK19) {
        gun({0.0f,  0.00f, 0.10f}, {0.05f, 0.07f, 0.22f}, gunMetal);  // slide
        gun({0.0f, -0.10f, 0.04f}, {0.05f, 0.14f, 0.06f}, gunDark);   // grip
    } else {                                                          // Uzi
        gun({0.0f,  0.02f, 0.16f}, {0.08f, 0.11f, 0.40f}, gunMetal);  // receiver + barrel
        gun({0.0f, -0.14f, 0.06f}, {0.05f, 0.22f, 0.05f}, gunDark);   // magazine
    }
    // Hand targets from the shared anchor table (weapon_visual.h): the support hand
    // reaches further on a longer gun, so each weapon's grips live in one place.
    const WeaponVisual& wv = weaponVisual(weaponId);
    glm::vec3 gripFire = wv.tpFireGrip, gripSupport = wv.tpSupportGrip;
    auto holdArm = [&](int shoulderBone, glm::vec3 gripLocal) {
        glm::vec3 S = glm::vec3(skel.world[shoulderBone][3]);          // shoulder = upper-arm joint
        glm::vec3 T = glm::vec3(hold * glm::vec4(gripLocal, 1.0f));
        glm::vec3 E = ikJoint(S, T, upArm, foreArm, glm::vec3(0, -1, 0));
        drawSegment(r, S, E, 0.12f, limb);                            // upper arm
        drawSegment(r, E, T, 0.11f, limb);                            // forearm
        r.drawCubeModel(glm::translate(glm::mat4(1.0f), T)
                      * glm::scale(glm::mat4(1.0f), glm::vec3(0.13f, 0.12f, 0.13f)), handCol);
    };
    holdArm(BONE_UPPERARM_L, gripFire);                               // firing arm
    holdArm(BONE_UPPERARM_R, gripSupport);                            // support arm

    // --- Legs: feet planted on the terrain, stepping over the stride and lifting in an
    // arc; knees bend toward facing. Hip anchors come from the thigh-bone joints. ---
    // Total 0.86 = standing thigh-joint height above the feet (pelvis at s*0.95, thigh
    // joint at -0.09), so standing legs are straight. Bones aren't scaled by s, so a low
    // (crouch) hip or a lifted swing foot still bends the knee.
    const float thighLen = 0.43f, shinLen = 0.43f;
    const float yr  = glm::radians(yaw);
    const glm::vec3 fwd = {cosf(yr), 0.0f, sinf(yr)};
    bool  airborne = pos.y > terrainHeight(pos.x, pos.z) + 0.05f;
    float stride = (crouched ? 0.18f : 0.38f) * amp;
    float lift   = (crouched ? 0.07f : 0.16f) * amp;
    auto leg = [&](int hipBone, float ph) {
        glm::vec3 hip = glm::vec3(skel.world[hipBone][3]);
        glm::vec3 foot;
        if (airborne) {
            foot = hip + fwd * (sinf(ph) * 0.10f) - glm::vec3(0, 0.55f, 0);
        } else {
            glm::vec3 fxz = hip + fwd * (sinf(ph) * stride);
            float up = fmaxf(0.0f, sinf(ph + 1.5708f)) * lift;
            foot = glm::vec3(fxz.x, terrainHeight(fxz.x, fxz.z) + up, fxz.z);
        }
        glm::vec3 knee = ikJoint(hip, foot, thighLen, shinLen, fwd);
        drawSegment(r, hip, knee, 0.20f, limb);                       // thigh
        drawSegment(r, knee, foot, 0.18f, limb);                      // shin
        drawSegment(r, foot, foot + fwd * 0.16f, 0.12f, limb);        // foot, pointing ahead
    };
    leg(BONE_THIGH_L, phase);
    leg(BONE_THIGH_R, phase + 3.14159f);
}

// Training-only mirror on the range wall: a stencil-masked planar reflection (no FBO).
// Mark the glass into the stencil, reset depth there, then redraw the world reflected
// across the wall plane with the cull winding flipped. The local player IS drawn here
// (unlike the first-person pass), so you can see yourself. Must run after the wall is
// drawn this frame and before the HUD.
static void drawMirror(Renderer& r, const Camera& cam, const GameState& gs, int localID,
                       const float* walkPhase, const float* walkAmp, const float* adsAnim) {
    const float     planeX = RANGE_WALL_FACE;               // reflect across the wall face
    const glm::vec3 mc     = {planeX - 0.02f, 1.70f, 0.0f};  // glass center, dead ahead
    const glm::vec3 mscale = {0.04f, 3.60f, 2.60f};         // ~3.6 m tall, 2.6 m wide
    const float     hh = mscale.y * 0.5f, hw = mscale.z * 0.5f;
    const glm::mat4 proj = cam.proj(r.aspect());

    // 1. Mark the glass rectangle into the stencil (no color, no depth write). REPLACE
    //    only on depth-pass, so anything in front of the glass correctly masks it out.
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    r.drawCube(mc, mscale, glm::vec3(0.0f));

    // 2. Reset depth to far inside the glass so the reflected geometry (which lives
    //    behind the wall in world space) isn't occluded by the wall drawn this frame.
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);
    r.fillDepthFar();                       // clobbers view/proj uniforms (restored below)
    glDepthFunc(GL_LESS);

    // 3. Reflected world, only where stencil==1. Reflection flips winding -> cull front.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glm::mat4 R(1.0f); R[0][0] = -1.0f; R[3][0] = 2.0f * planeX;  // x -> 2*planeX - x
    r.shader.setMat4(r.shader.locProj, proj);
    r.setView(cam.view() * R);
    glCullFace(GL_FRONT);

    r.drawGround();
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!(gs.usedMask & (1u << i)) || !gs.players[i].alive) continue;
        const Player& pl = gs.players[i];
        drawPlayerSkeleton(r, pl.pos, pl.yaw, pl.pitch, pl.lean, pl.weaponId, pl.crouched,
                           adsAnim[i], walkPhase[i], walkAmp[i],
                           i == localID ? COLOR_SELF : COLOR_ENEMY);
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet& b = gs.bullets[i];
        if (b.active)
            r.drawCube(b.pos, {0.1f, 0.1f, 0.1f},
                       b.ownerID == localID ? COLOR_BULLET_OWN : COLOR_BULLET_ENEMY);
    }

    glCullFace(GL_BACK);
    glDisable(GL_STENCIL_TEST);
    r.setView(cam.view());

    // 4. Dark frame around the glass + a faint cool sheen, so it reads as a mirror.
    const glm::vec3 frameCol = {0.09f, 0.09f, 0.11f};
    const float fx = planeX - 0.05f, ft = 0.07f;
    r.drawCube({fx, mc.y + hh, mc.z}, {ft, 0.12f, 2 * hw + 0.18f}, frameCol);  // top
    r.drawCube({fx, mc.y - hh, mc.z}, {ft, 0.12f, 2 * hw + 0.18f}, frameCol);  // bottom
    r.drawCube({fx, mc.y, mc.z - hw}, {ft, 2 * hh + 0.18f, 0.12f}, frameCol);  // left
    r.drawCube({fx, mc.y, mc.z + hw}, {ft, 2 * hh + 0.18f, 0.12f}, frameCol);  // right
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    r.shader.setFloat(r.shader.locAlpha, 0.10f);
    r.drawCube(mc, mscale, {0.55f, 0.68f, 0.80f});       // glass sheen
    r.shader.setFloat(r.shader.locAlpha, 1.0f);
    glDisable(GL_BLEND);
}

// Shadow casters + receivers: ground/terrain, cover boxes, remote players, bullets.
// Drawn twice per frame — once into the sun's depth map, once into the main view —
// so it must contain only real world geometry (no HUD, viewmodel, mirror, or the
// cosmetic ground blobs / aim cross, which are added in the main pass only).
static void drawWorldGeometry(Renderer& r, const GameState& gs, int localID,
                              const float* walkPhase, const float* walkAmp,
                              const float* adsAnim, bool showHitboxes) {
    if (gMapId == MAP_FIELD) r.drawTerrain(); else r.drawGround();

    for (int i = 0; i < gMapBoxCount; i++) {
        const Box& b = gMapBoxes[i];
        r.drawCube(b.center, b.half * 2.0f, mapBoxMaterial(i));
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == localID) continue;
        if (!(gs.usedMask & (1u << i))) continue;
        if (!gs.players[i].alive) continue;
        const Player& pl = gs.players[i];
        const glm::vec3& p = pl.pos;
        if (showHitboxes) {
            // Debug: draw the actual gameplay hit regions, color-coded by multiplier.
            static const glm::vec3 regionCol[MAX_HIT_REGIONS] = {
                {0.20f, 0.45f, 0.95f},   // legs  (0.8x) — blue
                {0.20f, 0.85f, 0.25f},   // torso (1.0x) — green
                {0.95f, 0.70f, 0.15f},   // neck  (1.5x) — amber
                {0.95f, 0.20f, 0.20f},   // head  (2.0x) — red
                {0.70f, 0.30f, 0.90f},   // arms  (1.0x) — purple
            };
            HitRegion rg[MAX_HIT_REGIONS];
            int nr = playerHitRegions(p, pl.crouched, pl.yaw, pl.ads, rg);
            for (int k = 0; k < nr; k++)
                r.drawCube(rg[k].center, rg[k].half * 2.0f, regionCol[k]);
        } else {
            drawPlayerSkeleton(r, p, pl.yaw, pl.pitch, pl.lean, pl.weaponId, pl.crouched,
                               adsAnim[i], walkPhase[i], walkAmp[i], COLOR_ENEMY);
        }
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet& b = gs.bullets[i];
        if (!b.active) continue;
        bool own = b.ownerID == localID;
        r.drawCube(b.pos, {0.1f, 0.1f, 0.1f}, own ? COLOR_BULLET_OWN : COLOR_BULLET_ENEMY);
    }
}

static void renderScene(Renderer& r, const Camera& cam, const GameState& gs, int localID,
                        const HudState& hud, bool scoreboard, bool online,
                        const ViewModel& vm,
                        const Decal* decals, int decalCount, bool drawRange,
                        const ConnectPrompt& connectPrompt,
                        const float* walkPhase, const float* walkAmp,
                        const float* adsAnim, bool showHitboxes) {
    static const glm::vec3 COLOR_BLOB = {0.16f, 0.27f, 0.16f};  // ground, darkened

    // Pass 1: scene depth from the sun, focused on the camera (near-field shadows).
    r.beginShadowPass(cam.eye);
    drawWorldGeometry(r, gs, localID, walkPhase, walkAmp, adsAnim, showHitboxes);
    r.endShadowPass();

    // Pass 2: lit main view, sampling the shadow map built above.
    r.beginFrame(cam.view(), cam.proj(r.aspect()), cam.eye);
    r.drawSky(cam.view(), cam.proj(r.aspect()));
    drawWorldGeometry(r, gs, localID, walkPhase, walkAmp, adsAnim, showHitboxes);

    if (drawRange) {
        // the wall itself is a training-map box (drawn by the map loop). Just the
        // aim reference: a red cross at standing eye height, offset left of the mirror
        // (which sits dead ahead at z=0) so the two don't overlap.
        glm::vec3 c = {RANGE_WALL_FACE - 0.03f, RANGE_BULLSEYE_Y, -7.0f};
        r.drawCube(c, {0.06f, 1.0f, 0.10f}, {0.85f, 0.25f, 0.20f});
        r.drawCube(c, {0.06f, 0.10f, 1.0f}, {0.85f, 0.25f, 0.20f});
    }
    // Bullet-impact decals on every surface (online + offline), oriented to the hit face.
    for (int i = 0; i < decalCount; i++)
        drawDecal(r, decals[i]);
    // Bullet ground blobs (real shadows replace the old per-player blob).
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet& b = gs.bullets[i];
        if (!b.active) continue;
        r.drawCube({b.pos.x, terrainHeight(b.pos.x, b.pos.z) + 0.01f, b.pos.z},
                   {0.22f, 0.001f, 0.22f}, COLOR_BLOB);
    }
    if (drawRange) drawMirror(r, cam, gs, localID, walkPhase, walkAmp, adsAnim);
    // First-person gun is an overlay — don't world-shadow it (FPS convention).
    if (gs.players[localID].alive && !connectPrompt.open) {
        r.shader.setInt(r.shader.locUseShadow, 0);
        drawViewModel(r, cam, vm);
        r.shader.setInt(r.shader.locUseShadow, 1);
    }
    drawHUD(r, gs, localID, hud, scoreboard, online);
    drawConnectPrompt(r, connectPrompt);
    r.endFrame();
}

// Debug: FPS_SHOT=<path.ppm> dumps the framebuffer once, shortly after start.
// Drawable size, not window size — they differ on HiDPI displays.
static void dumpFrame(const Renderer& r, const char* path) {
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(r.window, &w, &h);
    unsigned char* px = (unsigned char*)malloc((size_t)w * h * 3);  // one-shot debug path
    if (!px) return;
    glReadBuffer(GL_FRONT);  // called after the swap; back buffer is undefined
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    glReadBuffer(GL_BACK);
    FILE* f = fopen(path, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; y--)        // GL reads bottom-up
            fwrite(&px[(size_t)y * w * 3], 1, (size_t)w * 3, f);
        fclose(f);
        printf("frame dumped to %s\n", path);
    }
    free(px);
}

static const char* DEFAULT_SERVER_IP = "127.0.0.1";

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered so logs flush when piped to a file
    platformSocketInit();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    Renderer renderer;
    if (!renderer.init("pvp_shooter", 1280, 720)) {
        fprintf(stderr, "renderer init failed\n");
        return 1;
    }
    SDL_SetRelativeMouseMode(SDL_TRUE);

    Audio audio;
    audio.init();   // runs silent if no device

    static GameState offline;             // local practice match vs dummy
    offline.usedMask = 0b11;              // slot 0 = self, slot 1 = dummy
    offline.players[0].pos = {12, 0, 0};  // on the firing line, facing the range wall
    offline.players[1].pos = {24, 0, 6};  // dummy off to the side, clear of the pattern
    static GameState display;             // what gets rendered when online

    // Persistent bullet-impact decals on all world surfaces (offline + online).
    static Decal decals[DECAL_MAX];
    int decalCount = 0;       // live decals (<= DECAL_MAX)
    int decalHead  = 0;       // next write index (ring buffer)
    auto addDecal = [&](const glm::vec3& pos, const glm::vec3& normal) {
        decals[decalHead] = {pos, normal};
        decalHead = (decalHead + 1) % DECAL_MAX;
        if (decalCount < DECAL_MAX) decalCount++;
    };

    ClientNet net;
    Player    predicted;                  // own player, client-side predicted
    uint32_t  appliedStateSeq = 0;
    glm::vec3 posError(0.0f);             // smooths the snap to authoritative pos
    const float PRED_SMOOTH_TAU = 0.08f;  // correction half-life (~smoothing window)

    Camera     cam;
    cam.yaw = 0.0f;  // offline spawn looks down the range (+X) at the target wall
    FrameInput input;


    // Default: training mode (offline). Only auto-connect when an IP arg is given;
    // otherwise press C to connect to a server.
    if (argc > 1) {
        printf("connecting to %s...\n", argv[1]);
        if (!net.connect(argv[1]))
            printf("connect failed — training mode\n");
    } else {
        printf("training mode — press C to connect to a server\n");
    }

    const float FIXED_DT    = 1.0f / PHYS_HZ;
    float       accumulator = 0.0f;
    Uint64      last        = SDL_GetPerformanceCounter();
    bool        running     = true;
    HudState    hud;
    int         prevOwnHP   = PLAYER_HP;
    bool        prevOwnAlive = true;
    bool        offlineShoot = false;  // latch click until a physics tick consumes it
    ViewModel   vm;                     // first-person gun state
    float       stepTimer    = 0.0f;    // footstep cadence
    float       prevOwnPosY  = 0.0f;    // detect airborne (no footsteps in air)
    uint8_t     prevRemoteShots[MAX_PLAYERS] = {0};  // last seen authoritative per-player shot counters
    bool        remoteShotsSeeded = false;           // seed once when entering online play
    float       fpsAvg       = 0.0f;    // smoothed FPS readout
    int         fireMode     = FIRE_SEMI;
    float       fireTimer    = 0.0f;    // cooldown until next allowed shot
    int         burstRemaining = 0;     // rounds left in current burst
    bool        prevReloading = false;  // for reload-start sound
    float       gameTime     = 0.0f;    // accumulated seconds, drives grass wind
    uint8_t     prevOwnHits  = 0;       // hit-marker: own hits-dealt counter
    glm::vec3   hitMarkerPos(0.0f);     // world impact point of the latest hit
    float       recoilHeat   = 0.0f;    // ramps recoil kick over a sustained spray
    float       sinceShot    = 1e9f;    // seconds since last shot (recovery gating)

    bool wasOnline = false;
    ConnectPrompt connectPrompt;
    bool connectPromptActive = false;

    // Per-remote-player walk animation state (client-side, cosmetic). Phase advances
    // with horizontal speed derived from the interpolated render positions.
    float     walkPhase[MAX_PLAYERS] = {0};
    float     walkAmp[MAX_PLAYERS]   = {0};   // smoothed 0..1 move amount
    float     adsAnim[MAX_PLAYERS]   = {0};   // smoothed 0..1 aim-down-sights pose
    float     walkSpeed[MAX_PLAYERS] = {0};   // smoothed horizontal speed (m/s)
    glm::vec3 prevPlayerPos[MAX_PLAYERS];
    bool      prevPosValid[MAX_PLAYERS] = {false};
    bool      showHitboxes = false;           // H: draw color-coded hit regions

    auto closeConnectPrompt = [&]() {
        if (!connectPromptActive) return;
        connectPrompt.close();
        SDL_StopTextInput();
        SDL_SetRelativeMouseMode(SDL_TRUE);
        connectPromptActive = false;
    };

    printf("controls: WASD move, mouse look, LMB shoot, Q/E lean, 1/2 or scroll weapon "
           "(Uzi/Glock), C connect, F wireframe, H hitboxes, ESC quit\n");
    printf("offline shooting range: fire at the wall to see your spread; G clears the marks\n");

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = (float)(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (dt > 0.0f) {              // smoothed FPS from raw frame time (before the cap)
            float inst = 1.0f / dt;
            fpsAvg = fpsAvg <= 0.0f ? inst : fpsAvg + (inst - fpsAvg) * 0.1f;
            hud.fps = fpsAvg;
        }
        if (dt > 0.05f) dt = 0.05f;   // cap to avoid spiral

        pollInput(input, cam, &connectPrompt);
        if (input.quit) running = false;
        if (input.wireframeToggle) renderer.toggleWireframe();
        if (input.hitboxToggle) showHitboxes = !showHitboxes;
        if (input.clearRange) { decalCount = 0; decalHead = 0; }

        // Weapon select (1 = Uzi, 2 = Glock). Offline re-arms now; online the server
        // adopts it from the input packet and stays authoritative.
        if (input.weaponSelect >= 0 && (uint8_t)input.weaponSelect != gWeaponId) {
            gWeaponId = (uint8_t)input.weaponSelect;
            giveWeapon(offline.players[0], gWeaponId);
        }
        input.state.weaponId = gWeaponId;

        // Lean (Q/E): smooth toward the held direction; the result drives the camera
        // roll/peek and is sent to the server for the authoritative shot origin.
        float leanTarget = (input.state.leanRight ? 1.0f : 0.0f) -
                           (input.state.leanLeft  ? 1.0f : 0.0f);
        cam.updateLean(leanTarget, dt);
        input.state.lean = cam.lean;

        if (connectPromptActive && !connectPrompt.open) closeConnectPrompt();

        // C opens the modal anytime — even mid-connect or already connected, so you
        // can switch servers without quitting. Game keeps running behind the modal.
        if (input.connectRequested && !connectPrompt.open) {
            connectPrompt.show(DEFAULT_SERVER_IP);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_StartTextInput();
            connectPromptActive = true;
        }

        if (connectPrompt.open && input.connectSubmit) {
            const char* ip = connectPrompt.ip[0] ? connectPrompt.ip : DEFAULT_SERVER_IP;
            closeConnectPrompt();
            printf("connecting to %s...\n", ip);
            if (!net.connect(ip))
                printf("connect failed — offline practice mode\n");
        }


        net.update(dt);

        bool online  = net.connected && net.hasState;
        int  localID = online ? net.playerID : 0;
        if (localID < 0 || localID >= MAX_PLAYERS) localID = 0;

        if (online && !wasOnline) {   // reset offline→online client state
            // Adopt whatever map the server advertised in its ACCEPT (falls back to
            // warehouse if somehow unset), so the client always matches the server.
            MapId srv = (net.serverMap >= 0 && net.serverMap < 3) ? (MapId)net.serverMap
                                                                  : MAP_WAREHOUSE;
            setMap(srv);
            prevOwnHP       = PLAYER_HP;
            prevOwnHits     = 0;
            prevOwnAlive    = true;
            appliedStateSeq = 0;
            predicted       = Player{};
            posError        = glm::vec3(0.0f);
            decalCount      = 0;
            decalHead       = 0;
            remoteShotsSeeded = false;
        }
        if (!online && wasOnline) setMap(MAP_TRAINING);  // back to the practice arena
        if (!online) remoteShotsSeeded = false;
        wasOnline = online;

        // --- fire control: pick shots this frame by fire mode, gate on ammo ---
        const WeaponDef& lw = weaponDef(gWeaponId);   // local (selected) weapon
        if (lw.semiOnly) fireMode = FIRE_SEMI;        // pistols: semi only
        else if (input.fireModeToggle) { fireMode = (fireMode + 1) % FIRE_MODE_COUNT; burstRemaining = 0; }
        hud.fireMode = fireMode;

        bool ownAliveF, ownReloadingF; int ownMagF;   // own weapon state for gating
        if (online) {
            const PlayerNetState& o = net.lastState.players[localID];
            ownAliveF = o.alive != 0; ownMagF = o.mag; ownReloadingF = o.reloading != 0;
        } else {
            const Player& s = offline.players[0];
            ownAliveF = s.alive; ownMagF = s.mag; ownReloadingF = s.reloading;
        }

        fireTimer -= dt;
        if (fireMode == FIRE_BURST && input.state.shoot) burstRemaining = lw.burstCount;
        bool  wantFire = false;
        float fireInt  = lw.fireSemiInt;
        if (fireMode == FIRE_SEMI)  { wantFire = input.state.shoot;     fireInt = lw.fireSemiInt;  }
        if (fireMode == FIRE_BURST) { wantFire = burstRemaining > 0;    fireInt = lw.fireBurstInt; }
        if (fireMode == FIRE_AUTO)  { wantFire = input.state.shootHeld; fireInt = lw.fireAutoInt;  }

        bool fired = wantFire && ownAliveF && ownMagF > 0 && !ownReloadingF && fireTimer <= 0.0f;
        if (online) offlineShoot = false;
        if (fired) {
            fireTimer = fireInt;
            if (fireMode == FIRE_BURST && burstRemaining > 0) burstRemaining--;
            if (online) net.shotSeq++;   // reliable shot; server spawns + decrements mag
            else        offlineShoot = true;
            vm.flashTimer = 0.05f;
            vm.recoilT    = 1.0f;
            audioPlay(SND_SHOOT);
        }
        if (input.state.shoot && ownAliveF && ownMagF == 0 && !ownReloadingF) audioPlay(SND_DRYFIRE);
        if (ownReloadingF && !prevReloading) audioPlay(SND_RELOAD);
        prevReloading = ownReloadingF;

        // Report input (incl. this frame's shotSeq) with the pre-kick aim, so the
        // shot leaves the barrel exactly where aimed; the kick lands afterwards.
        if (net.connected) {
            uint32_t viewSeq  = 0;
            uint8_t  viewFrac = 0;
            if (net.hasState) {
                float ps = net.playSeq < 0.0f ? 0.0f : net.playSeq;  // playout pos = render time
                viewSeq  = (uint32_t)floorf(ps);
                viewFrac = (uint8_t)((ps - (float)viewSeq) * 255.0f + 0.5f);
            }
            net.sendInput(input.state, viewSeq, viewFrac);
        }

        // Recoil: kick the aim up + a little sideways, escalating with heat. Applied
        // after the shot is sent so each shot carries only prior shots' kicks.
        if (fired) {
            float t    = recoilHeat / RECOIL_HEAT_CAP;
            float mult = input.state.ads ? RECOIL_ADS_MULT : RECOIL_HIP_MULT;
            if (recoilHeat == 0.0f) mult *= RECOIL_FIRST_MULT;  // snappier opening shot
            float rs   = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;  // [-1,1]
            float ramp = glm::min(t, 1.0f);
            float vKick = glm::mix(RECOIL_PITCH_MIN, RECOIL_PITCH_MAX, ramp) * mult;
            if (t > 1.0f) vKick += (t - 1.0f) * RECOIL_HEAT_OVER * mult;
            float hKick = RECOIL_YAW * (0.6f + 0.8f * ramp) * mult * rs;
            if (t > 1.0f) hKick *= 1.0f + (t - 1.0f) * 0.05f;
            cam.applyRecoil(vKick, hKick);
            recoilHeat += 1.0f;
            sinceShot = 0.0f;
        }
        sinceShot += dt;
        if (sinceShot > RECOIL_HEAT_RESET) recoilHeat = 0.0f;
        cam.recoverRecoil(dt, sinceShot < RECOIL_RECOVER_DELAY);

        // fixed-step simulation
        accumulator += dt;
        while (accumulator >= FIXED_DT) {
            if (online) {
                if (prevOwnAlive) movePlayer(predicted, input.state, FIXED_DT);
            } else {
                Player& self  = offline.players[0];
                Player& dummy = offline.players[1];
                if (offlineShoot && self.alive) {
                    glm::vec3 origin, dir;
                    float spread = aimSpread(self, input.state);
                    weaponShot(input.state.yaw, input.state.pitch, cam.eyePos(), input.state.ads,
                               spread, origin, dir);
                    spawnBullet(offline, origin, dir, 0);
                    offlineShoot = false;
                }
                movePlayer(self, input.state, FIXED_DT);   // wall push-out via collideXZ
                updateReload(self, input.state.reload, FIXED_DT);
                if (self.mag == 0 && self.reserve == 0)                       // keep practice stocked
                    self.reserve = weaponDef(self.weaponId).reservePerLife;
                // updateBullets stops rounds on any surface and reports the impacts
                // (pos + face normal); stamp a decal on each, exactly as the server does
                // online. Wall, cover, ground — every surface marks the same way.
                Impact imp[NET_MAX_IMPACTS];
                int impCount = 0;
                updateBullets(offline, FIXED_DT, nullptr, nullptr, imp, &impCount, NET_MAX_IMPACTS);
                for (int k = 0; k < impCount; k++) addDecal(imp[k].pos, imp[k].normal);
                if (!dummy.alive) {                                 // offline dummy respawn
                    dummy.respawnTimer -= FIXED_DT;
                    if (dummy.respawnTimer <= 0.0f) {
                        dummy = Player{};
                        dummy.pos = {24, 0, 6};
                    }
                }
            }
            accumulator -= FIXED_DT;
        }

        // Mirror self (training): movePlayer only sets yaw + crouch, so reflect aim
        // pitch, lean, and ADS onto the offline avatar too — that's what the range
        // mirror renders for your own reflection.
        offline.players[0].pitch = input.state.pitch;
        offline.players[0].lean  = input.state.lean;
        offline.players[0].ads   = input.state.ads;

        const GameState* shown = &offline;
        if (online) {
            // Reconcile prediction with each new authoritative state. The server
            // position trails our prediction by ~RTT, so snapping straight to it
            // jolts the camera under latency. Instead keep the corrected position
            // for the sim, but roll the jump into posError and decay it out so the
            // render eases over a few frames while local input stays responsive.
            if (net.lastState.seq != appliedStateSeq) {
                appliedStateSeq = net.lastState.seq;
                const PlayerNetState& own = net.lastState.players[net.playerID];
                glm::vec3 authoritative = {own.x, own.y, own.z};
                posError += predicted.pos - authoritative;
                predicted.pos = authoritative;
                if (glm::length(posError) > 2.0f) posError = glm::vec3(0.0f);  // big desync: snap
                hud.noteState(net.lastState);
            }
            StatePacket sa, sb;
            float alpha = 0.0f;
            net.sampleRender(sa, sb, alpha);   // playout-buffered: smooth under jitter/loss
            unpackState(sa, sb, alpha, display);
            display.players[localID].pos = predicted.pos + posError;  // smoothed own view
            shown = &display;
        }
        posError *= expf(-dt / PRED_SMOOTH_TAU);
        if (glm::length(posError) < 0.0005f) posError = glm::vec3(0.0f);

        // Drain server-sent world impacts into decals (online stamping path).
        for (int k = 0; k < net.impactQCount; k++) {
            const ImpactNetState& im = net.impactQ[k];
            addDecal({im.x, im.y, im.z}, IMPACT_DIRS[im.dir < 6 ? im.dir : IMP_PY]);
        }
        net.impactQCount = 0;

        float eyeH = shown->players[localID].crouched ? CROUCH_EYE : EYE_HEIGHT;
        cam.eye = shown->players[localID].pos + glm::vec3(0, eyeH, 0);

        const Player& own = shown->players[localID];
        if (own.hp < prevOwnHP && own.alive) hud.flashTimer = 0.4f;  // got hit
        prevOwnHP = own.hp;
        hud.flashTimer -= dt;
        if (hud.flashTimer < 0.0f) hud.flashTimer = 0.0f;

        // Hit marker: arm when our own hits-dealt counter advances, then keep the
        // world impact point projected to screen for the marker's lifetime. The
        // delta < 128 test ignores counter resets/wraps (respawn, offline->online).
        uint8_t curHits = online ? net.lastState.recvHits : (uint8_t)offline.players[0].hits;
        glm::vec3 impact = online
            ? glm::vec3(net.lastState.recvHitX, net.lastState.recvHitY, net.lastState.recvHitZ)
            : offline.players[0].lastHitPos;
        uint8_t hitDelta = (uint8_t)(curHits - prevOwnHits);
        if (hitDelta != 0 && hitDelta < 128) {
            hitMarkerPos       = impact;
            hud.hitMarkerTimer = HIT_MARKER_TIME;
        }
        prevOwnHits = curHits;
        if (hud.hitMarkerTimer > 0.0f) {
            glm::vec4 clip = cam.proj(renderer.aspect()) * cam.view() * glm::vec4(hitMarkerPos, 1.0f);
            hud.hitMarkerOnScreen = clip.w > 1e-4f;
            if (hud.hitMarkerOnScreen) hud.hitMarkerNDC = {clip.x / clip.w, clip.y / clip.w};
        }
        hud.hitMarkerTimer -= dt;
        if (hud.hitMarkerTimer < 0.0f) hud.hitMarkerTimer = 0.0f;

        if (prevOwnAlive && !own.alive) {
            printf("you died — respawning\n");
            hud.deathTimer = RESPAWN_TIME;
            audioPlay(SND_DEATH);
        }
        if (!prevOwnAlive && own.alive) {
            printf("respawned\n");
            audioPlay(SND_RESPAWN);
        }
        prevOwnAlive = own.alive;
        hud.deathTimer -= dt;
        if (hud.deathTimer < 0.0f) hud.deathTimer = 0.0f;
        hud.feed.update(dt);

        // weapon viewmodel: ADS transition, FOV zoom, fire feedback
        float adsTarget = input.state.ads ? 1.0f : 0.0f;
        float k = dt * ADS_LERP_SPEED;
        if (k > 1.0f) k = 1.0f;
        vm.adsT += (adsTarget - vm.adsT) * k;
        cam.fov  = glm::mix(HIP_FOV, ADS_FOV, vm.adsT);
        vm.flashTimer -= dt;
        if (vm.flashTimer < 0.0f) vm.flashTimer = 0.0f;
        vm.recoilT -= dt * 8.0f;
        if (vm.recoilT < 0.0f) vm.recoilT = 0.0f;

        // footsteps while moving on the ground
        float dY      = own.pos.y - prevOwnPosY;
        bool  onGround = dY < 0.02f && dY > -0.02f;       // not climbing/falling
        bool  moving   = input.state.w || input.state.a || input.state.s || input.state.d;
        if (own.alive && moving && onGround) {
            stepTimer -= dt;
            if (stepTimer <= 0.0f) {
                audioPlay(SND_STEP, input.state.crouch ? 0.4f : 0.7f);
                stepTimer = input.state.sprint ? 0.28f : input.state.crouch ? 0.5f : 0.38f;
            }
        } else {
            stepTimer = 0.0f;                              // first step fires instantly
        }
        prevOwnPosY = own.pos.y;

        // Enemy gunshots: use per-player authoritative shot counters (from server).
        // This is robust under packet loss/jitter and independent from bullet replication.
        if (online) {
            const StatePacket& st = net.lastState;
            if (!remoteShotsSeeded) {
                for (int i = 0; i < MAX_PLAYERS; i++)
                    prevRemoteShots[i] = st.players[i].shotsFired;
                remoteShotsSeeded = true;
            }
            for (int i = 0; i < MAX_PLAYERS; i++) {
                uint8_t cur = st.players[i].shotsFired;
                uint8_t delta = (uint8_t)(cur - prevRemoteShots[i]);  // wrap-safe
                prevRemoteShots[i] = cur;
                if (i == localID) continue;
                if (!(st.usedMask & (1u << i))) continue;
                if (delta == 0 || delta >= 128) continue;  // ignore resets / stale jumps

                int plays = delta <= 4 ? (int)delta : 4;  // cap catch-up bursts
                for (int n = 0; n < plays; n++)
                    audioPlayAt(SND_SHOOT, shown->players[i].pos, cam.eye, cam.front());
            }
        }

        // Advance each visible player's walk animation from how fast their rendered
        // position moves on the ground. Includes the local player so the training
        // mirror's self-reflection animates while you walk.
        for (int i = 0; i < MAX_PLAYERS; i++) {
            bool show = (shown->usedMask & (1u << i)) && shown->players[i].alive;
            if (!show) { walkAmp[i] = 0.0f; adsAnim[i] = 0.0f; prevPosValid[i] = false; continue; }
            // Smooth the authoritative 0/1 ADS toward the held pose at the same rate as
            // the first-person ADS transition, so the raised gun eases in/out.
            float adsK = dt * ADS_LERP_SPEED; if (adsK > 1.0f) adsK = 1.0f;
            adsAnim[i] += ((shown->players[i].ads ? 1.0f : 0.0f) - adsAnim[i]) * adsK;
            glm::vec3 pp = shown->players[i].pos;
            float sp = 0.0f;
            if (prevPosValid[i]) {
                glm::vec3 d = pp - prevPlayerPos[i]; d.y = 0.0f;
                sp = glm::length(d) / (dt > 1e-4f ? dt : 1e-4f);
            }
            prevPlayerPos[i] = pp; prevPosValid[i] = true;
            walkSpeed[i] += (sp - walkSpeed[i]) * 0.30f;            // smooth the jitter
            float amp = walkSpeed[i] / MOVE_SPEED;
            walkAmp[i] = amp > 1.0f ? 1.0f : amp;
            walkPhase[i] += walkSpeed[i] * 1.8f * dt;               // stride tied to speed
        }

        gameTime += dt;
        renderer.setTime(gameTime);
        hud.adsT = vm.adsT;
        renderScene(renderer, cam, *shown, localID, hud, input.scoreboardHeld, online, vm,
                    decals, decalCount, !online, connectPrompt, walkPhase, walkAmp,
                    adsAnim, showHitboxes);

        static int frameCount = 0;
        const char* shotPath = getenv("FPS_SHOT");
        if (shotPath && ++frameCount == 60) { dumpFrame(renderer, shotPath); running = false; }
    }

    closeConnectPrompt();
    net.disconnect();
    audio.shutdown();
    renderer.shutdown();
    SDL_Quit();
    platformSocketCleanup();
    return 0;
}
