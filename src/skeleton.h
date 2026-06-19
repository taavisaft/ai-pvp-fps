#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "renderer.h"

// Generic bone-array skeleton for the blocky "box-man". Each bone is one cuboid,
// drawn through Renderer::drawCubeModel. Forward kinematics for now; the very same
// per-bone transforms will later be produced by a ragdoll solver (one rigid body per
// box), so the renderer never has to know which mode is driving the pose.
//
// Layout is built in makeBoxMan() below. Conventions:
//   jointOffset = this bone's pivot, expressed in the PARENT bone's joint space (rest).
//   localRot    = rotation applied AT the joint — the ONLY field animation/ragdoll write.
//   halfExtents = box half-size. The cube mesh spans [-0.5,0.5], so we scale by 2*half.
//   boxOffset   = box centre relative to this bone's own joint (limbs hang off the joint).
// Parents always precede children in the array, so one forward pass resolves everything.

enum BoneId {
    BONE_PELVIS = 0, BONE_TORSO, BONE_NECK, BONE_HEAD,
    BONE_UPPERARM_L, BONE_UPPERARM_R, BONE_LOWERARM_L, BONE_LOWERARM_R,
    BONE_THIGH_L,    BONE_THIGH_R,    BONE_SHIN_L,      BONE_SHIN_R,
    BONE_NOSE,   // facing marker on the head front (+Z is forward); parent precedes it
    BONE_COUNT
};

// localRot is last with an identity default so the rest-pose table can omit it
// (C++17 fills omitted trailing aggregate members from their default initializer).
struct Bone {
    int       parent;
    glm::vec3 jointOffset;
    glm::vec3 halfExtents;
    glm::vec3 boxOffset;
    glm::vec3 color;
    glm::quat localRot{1.0f, 0.0f, 0.0f, 0.0f};   // identity = rest pose (standing, arms down)
};

struct Skeleton {
    Bone      bones[BONE_COUNT];
    glm::mat4 world[BONE_COUNT];   // filled by computeWorld()

    // Resolve every bone's world matrix from `root` (places the pelvis joint in the
    // world: e.g. translate(feet) * translate(0,hipY,0) * rotateY(yaw)).
    void computeWorld(const glm::mat4& root) {
        for (int i = 0; i < BONE_COUNT; ++i) {
            glm::mat4 local = glm::translate(glm::mat4(1.0f), bones[i].jointOffset)
                            * glm::mat4_cast(bones[i].localRot);
            world[i] = (bones[i].parent < 0 ? root : world[bones[i].parent]) * local;
        }
    }

    void draw(Renderer& r) const {
        for (int i = 0; i < BONE_COUNT; ++i) {
            glm::mat4 m = world[i]
                        * glm::translate(glm::mat4(1.0f), bones[i].boxOffset)
                        * glm::scale(glm::mat4(1.0f), bones[i].halfExtents * 2.0f);
            r.drawCubeModel(m, bones[i].color);
        }
    }
};

// Procedural walk cycle: write thigh/shin/arm localRot from a single phase. Legs swing
// fore/aft about the hip X axis (opposite phase L/R), knees bend on the recovery half,
// arms counter-swing against the legs. `amp` scales the whole thing (0 = stand still),
// so a caller can drive it from movement speed. Rotations are about local X because the
// limbs hang along -Y and the figure faces +Z, so X is the left/right swing axis.
inline void poseWalk(Skeleton& s, float phase, float amp = 1.0f) {
    auto rotX = [](float a) { return glm::angleAxis(a, glm::vec3(1.0f, 0.0f, 0.0f)); };
    const float pi = glm::pi<float>();
    float L = phase, R = phase + pi;                  // legs in antiphase
    // Negative swing so the bent/lifted leg (knee flexes on this same phase) travels
    // FORWARD (+Z, toward the nose) — a positive swing puts the lift on the rearward
    // leg and reads as a moonwalk. Arms negate too, to stay counter to the legs.
    float thighSwing = -0.50f * amp;
    float kneeBend   =  0.95f * amp;
    float armSwing   = -0.40f * amp;
    s.bones[BONE_THIGH_L].localRot = rotX(thighSwing * sinf(L));
    s.bones[BONE_THIGH_R].localRot = rotX(thighSwing * sinf(R));
    // Knee only flexes one way: shin folds BACK (-Z, behind the +Z facing) as the leg
    // lifts. Rectified so it bends on the recovery half only.
    s.bones[BONE_SHIN_L].localRot  = rotX(kneeBend * fmaxf(0.0f, sinf(L + 1.2f)));
    s.bones[BONE_SHIN_R].localRot  = rotX(kneeBend * fmaxf(0.0f, sinf(R + 1.2f)));
    // Arms swing opposite their same-side leg (left arm forward when left leg back).
    s.bones[BONE_UPPERARM_L].localRot = rotX(armSwing * sinf(R));
    s.bones[BONE_UPPERARM_R].localRot = rotX(armSwing * sinf(L));
    // Elbows flex through the cycle so the forearm isn't rigid with the upper arm:
    // a slight base bend plus extra flex peaking as that arm swings forward. Scaled by
    // amp so it relaxes straight at rest. Phased with the same-side upper arm.
    float elbowBase  = 0.25f * amp;
    float elbowSwing = 0.55f * amp;
    s.bones[BONE_LOWERARM_L].localRot = rotX(-(elbowBase + elbowSwing * fmaxf(0.0f, sinf(R))));
    s.bones[BONE_LOWERARM_R].localRot = rotX(-(elbowBase + elbowSwing * fmaxf(0.0f, sinf(L))));
}

// Build the rest-pose humanoid (~1.88 m tall, pelvis joint at the origin of `root`).
// spineCol/armCol/legCol/headCol tint the chains so the hierarchy reads at a glance.
inline Skeleton makeBoxMan(glm::vec3 spineCol = {0.45f, 0.42f, 0.72f},
                           glm::vec3 armCol   = {0.18f, 0.62f, 0.50f},
                           glm::vec3 legCol   = {0.73f, 0.55f, 0.20f},
                           glm::vec3 headCol  = {0.90f, 0.78f, 0.62f}) {
    Skeleton s{};
    //                       parent            jointOffset            halfExtents             boxOffset            color
    s.bones[BONE_PELVIS]     = {-1,             { 0.00f,  0.00f, 0.0f}, {0.18f, 0.090f, 0.11f}, { 0.0f,  0.000f, 0.0f}, spineCol};
    s.bones[BONE_TORSO]      = {BONE_PELVIS,    { 0.00f,  0.09f, 0.0f}, {0.17f, 0.250f, 0.11f}, { 0.0f,  0.250f, 0.0f}, spineCol};
    s.bones[BONE_NECK]       = {BONE_TORSO,     { 0.00f,  0.50f, 0.0f}, {0.06f, 0.050f, 0.06f}, { 0.0f,  0.050f, 0.0f}, spineCol};
    s.bones[BONE_HEAD]       = {BONE_NECK,      { 0.00f,  0.10f, 0.0f}, {0.12f, 0.120f, 0.12f}, { 0.0f,  0.120f, 0.0f}, headCol};
    s.bones[BONE_UPPERARM_L] = {BONE_TORSO,     { 0.27f,  0.41f, 0.0f}, {0.07f, 0.180f, 0.07f}, { 0.0f, -0.180f, 0.0f}, armCol};
    s.bones[BONE_UPPERARM_R] = {BONE_TORSO,     {-0.27f,  0.41f, 0.0f}, {0.07f, 0.180f, 0.07f}, { 0.0f, -0.180f, 0.0f}, armCol};
    s.bones[BONE_LOWERARM_L] = {BONE_UPPERARM_L,{ 0.00f, -0.36f, 0.0f}, {0.06f, 0.180f, 0.06f}, { 0.0f, -0.180f, 0.0f}, armCol};
    s.bones[BONE_LOWERARM_R] = {BONE_UPPERARM_R,{ 0.00f, -0.36f, 0.0f}, {0.06f, 0.180f, 0.06f}, { 0.0f, -0.180f, 0.0f}, armCol};
    s.bones[BONE_THIGH_L]    = {BONE_PELVIS,    { 0.10f, -0.09f, 0.0f}, {0.08f, 0.215f, 0.08f}, { 0.0f, -0.215f, 0.0f}, legCol};
    s.bones[BONE_THIGH_R]    = {BONE_PELVIS,    {-0.10f, -0.09f, 0.0f}, {0.08f, 0.215f, 0.08f}, { 0.0f, -0.215f, 0.0f}, legCol};
    s.bones[BONE_SHIN_L]     = {BONE_THIGH_L,   { 0.00f, -0.43f, 0.0f}, {0.07f, 0.215f, 0.07f}, { 0.0f, -0.215f, 0.0f}, legCol};
    s.bones[BONE_SHIN_R]     = {BONE_THIGH_R,   { 0.00f, -0.43f, 0.0f}, {0.07f, 0.215f, 0.07f}, { 0.0f, -0.215f, 0.0f}, legCol};
    // Nose: small box on the head's front face (+Z). Reads which way the figure faces.
    s.bones[BONE_NOSE]       = {BONE_HEAD,      { 0.00f,  0.12f, 0.12f}, {0.030f, 0.030f, 0.045f}, { 0.0f, 0.0f, 0.045f}, {0.90f, 0.30f, 0.20f}};
    return s;
}
