#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "renderer.h"
#include "terrain.h"
#include "game.h"

// Client-side cosmetic ragdoll: a Position-Based-Dynamics particle chain that flops a
// dead player's body under gravity. Purely visual — the server just says "dead"; each
// client seeds one of these from the death position and simulates it locally. One
// particle per joint, Verlet integration, distance constraints for the bones plus a few
// non-drawn braces that keep the torso from shearing into spaghetti, and a ground plane
// so limbs pile on the terrain. Proportions read ~1.80 m tall (head top), matching the
// alive box-man. Renders the same box segments as the alive path (drawSegment clone).
//
// Lifecycle: seed() at the alive→dead transition, step() once per frame, draw() in the
// world pass (so it shadows). Reset active=false on respawn.

// Joints. Forward = +Z after the seed yaw (nose convention); +X is the figure's left.
enum RagId {
    RD_HEAD = 0, RD_NECK, RD_CHEST, RD_PELVIS,
    RD_SHL, RD_ELL, RD_HAL,   RD_SHR, RD_ELR, RD_HAR,
    RD_HIL, RD_KNL, RD_FOL,   RD_HIR, RD_KNR, RD_FOR,
    RD_COUNT
};

struct Ragdoll {
    struct Particle { glm::vec3 pos, prev; };
    struct Bone     { int a, b; float thick; glm::vec3 col; };     // drawn + constrained
    struct Link     { int a, b; float rest; };                     // distance constraint

    bool      active = false;
    Particle  p[RD_COUNT];
    glm::vec3 bodyCol{0.80f, 0.30f, 0.20f};

    // Visible bones (also become distance links). Thickness ~= the alive limb widths.
    static constexpr int BONE_COUNT = 11;
    Bone bones[BONE_COUNT];
    // Distance links: every bone above + torso braces (filled in seed()).
    static constexpr int LINK_COUNT = BONE_COUNT + 10;
    Link links[LINK_COUNT];
    int  linkCount = 0;

    // Rest-pose joint offsets in the figure's local frame (standing, feet ~y=0), before
    // the world translate + yaw. Head top = 1.80 m (head cube half 0.12), matching
    // STAND_HEIGHT and the alive box-man.
    static glm::vec3 restLocal(int i) {
        switch (i) {
            case RD_HEAD:  return { 0.00f, 1.68f, 0.0f};
            case RD_NECK:  return { 0.00f, 1.48f, 0.0f};
            case RD_CHEST: return { 0.00f, 1.42f, 0.0f};
            case RD_PELVIS:return { 0.00f, 0.98f, 0.0f};
            case RD_SHL:   return { 0.19f, 1.44f, 0.0f};
            case RD_ELL:   return { 0.21f, 1.16f, 0.0f};
            case RD_HAL:   return { 0.23f, 0.90f, 0.0f};
            case RD_SHR:   return {-0.19f, 1.44f, 0.0f};
            case RD_ELR:   return {-0.21f, 1.16f, 0.0f};
            case RD_HAR:   return {-0.23f, 0.90f, 0.0f};
            case RD_HIL:   return { 0.10f, 0.95f, 0.0f};
            case RD_KNL:   return { 0.10f, 0.50f, 0.0f};
            case RD_FOL:   return { 0.10f, 0.04f, 0.0f};
            case RD_HIR:   return {-0.10f, 0.95f, 0.0f};
            case RD_KNR:   return {-0.10f, 0.50f, 0.0f};
            case RD_FOR:   return {-0.10f, 0.04f, 0.0f};
        }
        return {0, 0, 0};
    }

    void addLink(int a, int b) {
        links[linkCount++] = {a, b, glm::length(p[a].pos - p[b].pos)};
    }

    // Place the body at `pos` (feet), rotated by `yaw` (degrees), tinted `col`. `vel` is
    // the body's world velocity at death so a running corpse carries momentum; a small
    // forward tip is added so it topples off its feet instead of balancing.
    void seed(const glm::vec3& pos, float yaw, const glm::vec3& col, const glm::vec3& vel) {
        bodyCol = col;
        float yr = 1.5707963f - glm::radians(yaw);          // same mapping as the alive root
        float c = cosf(yr), s = sinf(yr);
        glm::vec3 fwd = {sinf(glm::radians(yaw)), 0.0f, -cosf(glm::radians(yaw))};  // local +Z in world
        glm::vec3 tip = glm::length(vel) > 0.5f ? glm::normalize(glm::vec3(vel.x, 0, vel.z)) : fwd;

        for (int i = 0; i < RD_COUNT; ++i) {
            glm::vec3 L = restLocal(i);
            glm::vec3 w = pos + glm::vec3(c * L.x + s * L.z, L.y, -s * L.x + c * L.z);
            p[i].pos  = w;
            // prev behind pos → initial velocity. Uniform carry from `vel`; the upper
            // body gets an extra shove along `tip` so the figure pitches over.
            glm::vec3 v = vel;
            if (i == RD_HEAD || i == RD_NECK || i == RD_CHEST || i == RD_SHL || i == RD_SHR)
                v += tip * 1.5f;
            p[i].prev = w - v * (1.0f / 60.0f);
        }

        const glm::vec3 body = bodyCol, limb = bodyCol * 0.85f;
        bones[0]  = {RD_PELVIS, RD_CHEST, 0.24f, body};
        bones[1]  = {RD_CHEST,  RD_NECK,  0.17f, body};
        bones[2]  = {RD_NECK,   RD_HEAD,  0.10f, body};
        bones[3]  = {RD_SHL, RD_ELL, 0.11f, limb};
        bones[4]  = {RD_ELL, RD_HAL, 0.10f, limb};
        bones[5]  = {RD_SHR, RD_ELR, 0.11f, limb};
        bones[6]  = {RD_ELR, RD_HAR, 0.10f, limb};
        bones[7]  = {RD_HIL, RD_KNL, 0.18f, limb};
        bones[8]  = {RD_KNL, RD_FOL, 0.15f, limb};
        bones[9]  = {RD_HIR, RD_KNR, 0.18f, limb};
        bones[10] = {RD_KNR, RD_FOR, 0.15f, limb};

        linkCount = 0;
        for (int i = 0; i < BONE_COUNT; ++i) addLink(bones[i].a, bones[i].b);
        // Torso attach + anti-shear braces (not drawn) so the trunk stays a slab.
        addLink(RD_CHEST, RD_SHL); addLink(RD_CHEST, RD_SHR);
        addLink(RD_PELVIS, RD_HIL); addLink(RD_PELVIS, RD_HIR);
        addLink(RD_SHL, RD_SHR);   addLink(RD_HIL, RD_HIR);
        addLink(RD_SHL, RD_PELVIS); addLink(RD_SHR, RD_PELVIS);
        addLink(RD_HEAD, RD_CHEST);
        addLink(RD_NECK, RD_CHEST);
        active = true;
    }

    void step(float dt) {
        if (!active) return;
        if (dt > 0.033f) dt = 0.033f;                       // stability clamp
        const float damp = 0.99f;
        const glm::vec3 g = {0.0f, -GRAVITY, 0.0f};
        for (int i = 0; i < RD_COUNT; ++i) {
            glm::vec3 vel = (p[i].pos - p[i].prev) * damp;
            p[i].prev = p[i].pos;
            p[i].pos += vel + g * (dt * dt);
        }
        // Satisfy distance links, then clamp to the ground, a few iterations for rigidity.
        for (int it = 0; it < 8; ++it) {
            for (int k = 0; k < linkCount; ++k) {
                Particle& a = p[links[k].a];
                Particle& b = p[links[k].b];
                glm::vec3 d = b.pos - a.pos;
                float len = glm::length(d);
                if (len < 1e-5f) continue;
                float diff = (len - links[k].rest) / len * 0.5f;
                glm::vec3 corr = d * diff;
                a.pos += corr; b.pos -= corr;
            }
            for (int i = 0; i < RD_COUNT; ++i) {
                float gy = terrainHeight(p[i].pos.x, p[i].pos.z);
                if (p[i].pos.y < gy) {
                    p[i].pos.y = gy;
                    // Ground friction: kill the sliding by pulling prev toward pos.
                    p[i].prev.x += (p[i].pos.x - p[i].prev.x) * 0.5f;
                    p[i].prev.z += (p[i].pos.z - p[i].prev.z) * 0.5f;
                }
            }
        }
    }

    // Box spanning A..B, square cross-section `w` — same construction as main's
    // drawSegment, but rendered with the Blender body-part mesh for that bone.
    static void segment(Renderer& r, glm::vec3 A, glm::vec3 B, float w,
                        const glm::vec3& c, int part) {
        glm::vec3 mid = (A + B) * 0.5f, dir = B - A;
        float len = glm::length(dir);
        if (len < 1e-5f) return;
        dir /= len;
        glm::vec3 up = fabsf(dir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
        glm::vec3 bx = glm::normalize(glm::cross(up, dir));
        glm::vec3 bz = glm::cross(bx, dir);
        glm::mat4 m(1.0f);
        m[0] = glm::vec4(bx * w, 0.0f);
        m[1] = glm::vec4(dir * len, 0.0f);
        m[2] = glm::vec4(bz * w, 0.0f);
        m[3] = glm::vec4(mid, 1.0f);
        r.drawMeshModel(r.playerPart[part], m, c);
    }

    void draw(Renderer& r) const {
        if (!active) return;
        // Body-part mesh per drawn bone (indices match seed()'s bones[] order):
        // torso column, then arms, then legs.
        static constexpr int partOf[BONE_COUNT] = {
            Renderer::PART_TORSO, Renderer::PART_NECK, Renderer::PART_NECK,
            Renderer::PART_ARM, Renderer::PART_FOREARM, Renderer::PART_ARM, Renderer::PART_FOREARM,
            Renderer::PART_LEG, Renderer::PART_SHIN, Renderer::PART_LEG, Renderer::PART_SHIN,
        };
        for (int i = 0; i < BONE_COUNT; ++i)
            segment(r, p[bones[i].a].pos, p[bones[i].b].pos, bones[i].thick,
                    bones[i].col, partOf[i]);
        // Head mesh at the head particle (skin-toned like the alive model).
        glm::vec3 skin = {0.90f, 0.78f, 0.62f};
        r.drawMeshModel(r.playerPart[Renderer::PART_HEAD],
                        glm::translate(glm::mat4(1.0f), p[RD_HEAD].pos)
                      * glm::scale(glm::mat4(1.0f), glm::vec3(0.24f)), skin);
    }
};
