#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "skeleton.h"
#include "weapon_visual.h"
#include "game.h"
#include "terrain.h"

// Shared pose frames and coarse hit volumes: FK spine, IK arms onto weapon grips,
// weapon proxies, and IK legs. The client attaches detailed cosmetic meshes to these
// frames; the server maps the boxes to damage regions. Clothing and equipment do
// not exactly match the box outlines and do not add armor or collision protection.
// GL-free on purpose: the headless server includes this.
//
// The client may pass fractional crouch/ADS values for visual transitions; the server
// passes authoritative 0/1 values for hit regions. Walk phase/amp are also client-only,
// so those brief transitions and a running target's swinging legs are the remaining
// visual approximations around the authoritative pose.

enum PoseBoxPart : uint8_t {
    POSE_PELVIS, POSE_TORSO, POSE_NECK, POSE_HEAD, POSE_NOSE,
    POSE_ARM, POSE_HAND, POSE_GUN_METAL, POSE_GUN_DARK,
    POSE_LEG, POSE_FOOT,
};

// One oriented box: M's columns are the (unit) local axes + origin; the box spans
// ±half along them. Draw with M * scale(2*half) (unit cube mesh spans [-0.5, 0.5]);
// sweep by mapping the segment through inverse(M) and slab-testing against half.
struct PoseBox { glm::mat4 M; glm::vec3 half; uint8_t part; };
constexpr int MAX_POSE_BOXES = 24;

// 2-bone IK: given root joint S and end target T (world space) and bone lengths L1/L2,
// return the middle joint position. The joint bulges toward `bendHint` (the
// perpendicular component of it) — pass world-down for elbows, the body's forward for
// knees. The limb is then two boxes S->mid->T, correct at any yaw/pitch/lean.
inline glm::vec3 ikJoint(glm::vec3 S, glm::vec3 T, float L1, float L2, glm::vec3 bendHint) {
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
inline void poseSegment(glm::vec3 A, glm::vec3 B, float w, uint8_t part,
                        PoseBox out[], int& n) {
    glm::vec3 mid = (A + B) * 0.5f, dir = B - A;
    float len = glm::length(dir);
    if (len < 1e-5f) return;
    dir /= len;
    glm::vec3 up = fabsf(dir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    glm::vec3 bx = glm::normalize(glm::cross(up, dir));
    glm::vec3 bz = glm::cross(dir, bx);
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(bx, 0.0f);
    m[1] = glm::vec4(dir, 0.0f);
    m[2] = glm::vec4(bz, 0.0f);
    m[3] = glm::vec4(mid, 1.0f);
    out[n++] = {m, {w * 0.5f, len * 0.5f, w * 0.5f}, part};
}

inline int buildPlayerPose(const glm::vec3& pos, float yaw, float pitch, float lean,
                           uint8_t weaponId, float crouch, float ads,
                           float phase, float amp, PoseBox out[MAX_POSE_BOXES]) {
    static Skeleton skel = makeBoxMan();
    int n = 0;

    // Spine pose: lean roll on the torso, a little aim tilt on the neck. Limb-bone
    // localRots stay at rest — the limbs are IK'd below; we only read their joint
    // positions as anchors.
    float la = lean * glm::radians(LEAN_ANGLE_DEG);
    skel.bones[BONE_TORSO].localRot = glm::angleAxis(la, glm::vec3(0, 0, 1));
    skel.bones[BONE_NECK].localRot  = glm::angleAxis(glm::radians(-pitch) * 0.5f, glm::vec3(1, 0, 0));

    float crouchT = glm::clamp(crouch, 0.0f, 1.0f);
    // Keep every body part at its real size. Crouching lowers the pelvis by the
    // stance-height difference; the leg IK below keeps both feet planted and folds
    // the knees naturally instead of vertically shrinking the whole skeleton.
    float pelvisY = 0.95f - (STAND_HEIGHT - CROUCH_HEIGHT) * crouchT;
    glm::mat4 root = glm::translate(glm::mat4(1.0f), pos)
                   * glm::rotate(glm::mat4(1.0f), 1.5707963f - glm::radians(yaw), glm::vec3(0, 1, 0))
                   * glm::translate(glm::mat4(1.0f), glm::vec3(0, pelvisY, 0));
    skel.computeWorld(root);

    // Spine + head + nose from FK (the 8 limb bones are IK'd instead).
    const struct { int bone; uint8_t part; } spine[5] = {
        {BONE_PELVIS, POSE_PELVIS}, {BONE_TORSO, POSE_TORSO}, {BONE_NECK, POSE_NECK},
        {BONE_HEAD, POSE_HEAD}, {BONE_NOSE, POSE_NOSE}};
    for (auto sp : spine)
        out[n++] = {skel.world[sp.bone]
                        * glm::translate(glm::mat4(1.0f), skel.bones[sp.bone].boxOffset),
                    skel.bones[sp.bone].halfExtents, sp.part};

    // --- Weapon at the chest (pitched by aim), hung off the torso frame so it leans
    // and crouches with the body; both hands IK'd onto its grips. The walk bob fades
    // as you aim so the raised gun reads as steady. ---
    const float upArm = 0.30f, foreArm = 0.30f;
    float bob = sinf(phase) * amp * 0.03f * (1.0f - ads);
    glm::vec3 hipT = GUN_HOLD_HIP + glm::vec3(0.0f, bob, 0.0f);
    glm::mat4 hold = skel.world[BONE_TORSO]
        * glm::translate(glm::mat4(1.0f), glm::mix(hipT, GUN_HOLD_ADS, ads))
        * glm::rotate(glm::mat4(1.0f), -glm::radians(pitch), glm::vec3(1, 0, 0));
    auto gun = [&](glm::vec3 lp, glm::vec3 sz, uint8_t part) {
        out[n++] = {hold * glm::translate(glm::mat4(1.0f), lp), sz * 0.5f, part};
    };
    if (weaponId == WEP_GLOCK19) {
        gun({0.0f,  0.00f, 0.10f}, {0.05f, 0.07f, 0.22f}, POSE_GUN_METAL);  // slide
        gun({0.0f, -0.10f, 0.04f}, {0.05f, 0.14f, 0.06f}, POSE_GUN_DARK);   // grip
    } else {                                                                // Uzi
        gun({0.0f,  0.02f, 0.16f}, {0.08f, 0.11f, 0.40f}, POSE_GUN_METAL);  // receiver + barrel
        gun({0.0f, -0.14f, 0.06f}, {0.05f, 0.22f, 0.05f}, POSE_GUN_DARK);   // magazine
    }
    // Hand targets from the shared anchor table (weapon_visual.h): the support hand
    // reaches further on a longer gun, so each weapon's grips live in one place.
    const WeaponVisual& wv = weaponVisual(weaponId);
    auto holdArm = [&](int shoulderBone, glm::vec3 gripLocal) {
        glm::vec3 S = glm::vec3(skel.world[shoulderBone][3]);          // shoulder joint
        glm::vec3 T = glm::vec3(hold * glm::vec4(gripLocal, 1.0f));
        glm::vec3 E = ikJoint(S, T, upArm, foreArm, glm::vec3(0, -1, 0));
        poseSegment(S, E, 0.12f, POSE_ARM, out, n);                    // upper arm
        poseSegment(E, T, 0.11f, POSE_ARM, out, n);                    // forearm
        out[n++] = {glm::translate(glm::mat4(1.0f), T),
                    {0.065f, 0.06f, 0.065f}, POSE_HAND};               // fist on the grip
    };
    holdArm(BONE_UPPERARM_L, wv.tpFireGrip);                           // firing arm
    holdArm(BONE_UPPERARM_R, wv.tpSupportGrip);                        // support arm

    // --- Legs: feet planted on the terrain, stepping over the stride and lifting in
    // an arc; knees bend toward facing. Hip anchors come from the thigh-bone joints.
    // Total 0.86 = standing thigh-joint height above the feet (pelvis at 0.95, thigh
    // joint at -0.09), so standing legs are straight. Lowering the pelvis for crouch
    // makes the fixed-length thigh and shin fold at the knee. ---
    const float thighLen = 0.43f, shinLen = 0.43f;
    const float yr  = glm::radians(yaw);
    const glm::vec3 fwd = {cosf(yr), 0.0f, sinf(yr)};
    bool  airborne = pos.y > terrainHeight(pos.x, pos.z) + 0.05f;
    float stride = glm::mix(0.38f, 0.18f, crouchT) * amp;
    float lift   = glm::mix(0.16f, 0.07f, crouchT) * amp;
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
        poseSegment(hip, knee, 0.20f, POSE_LEG, out, n);               // thigh
        poseSegment(knee, foot, 0.18f, POSE_LEG, out, n);              // shin
        poseSegment(foot, foot + fwd * 0.16f, 0.12f, POSE_FOOT, out, n);  // foot
    };
    leg(BONE_THIGH_L, phase);
    leg(BONE_THIGH_R, phase + 3.14159f);
    return n;
}
