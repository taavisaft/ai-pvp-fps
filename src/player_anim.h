#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include "game.h"

// Client-only cosmetic animation inputs. The server poses with none of this, so
// hit regions stay the authoritative rest-stride pose.
struct PoseAnim {
    float phase   = 0.0f;   // gait cycle, radians
    float amp     = 0.0f;   // 0 still .. 1 walking
    float run     = 0.0f;   // 0 walk .. 1 sprint
    float idleT   = 0.0f;   // seconds standing, drives breathing and weight shift
    float airT    = -1.0f;  // seconds since takeoff, <0 on the ground
    float landT   = -1.0f;  // seconds since landing, <0 once recovered
    float velY    = 0.0f;
    float recoil  = 0.0f;   // 1 on fire, decays; pulls the gun back
    float flash   = 0.0f;   // >0 draws the muzzle flash
};

constexpr float WALK_CYCLE_M = 1.52f;
constexpr float RUN_CYCLE_M  = 2.60f;
constexpr float LAND_RECOVER_S = 0.28f;

struct PlayerAnimState {
    PoseAnim  pose;
    float     speed     = 0.0f;
    glm::vec3 prevPos{0.0f};
    bool      prevValid = false;
    bool      prevAir   = false;

    void reset() { *this = PlayerAnimState{}; }

    void update(const glm::vec3& pos, bool airborne, float velY, float dt) {
        float step = dt > 1e-4f ? dt : 1e-4f;
        float sp = 0.0f;
        if (prevValid) {
            glm::vec3 d = pos - prevPos; d.y = 0.0f;
            sp = glm::length(d) / step;
        }
        prevPos = pos; prevValid = true;
        speed += (sp - speed) * fminf(1.0f, dt * 18.0f);
        float walk = fminf(1.0f, speed / (MOVE_SPEED * 0.6f));
        float run  = glm::clamp((speed - MOVE_SPEED) / (SPRINT_SPEED - MOVE_SPEED), 0.0f, 1.0f);
        pose.amp += (walk - pose.amp) * fminf(1.0f, dt * 10.0f);
        pose.run += (run - pose.run) * fminf(1.0f, dt * 6.0f);
        float cycle = glm::mix(WALK_CYCLE_M, RUN_CYCLE_M, pose.run);
        pose.phase += speed * (6.2831853f / cycle) * dt;
        if (pose.phase > 62.831853f) pose.phase -= 62.831853f;
        pose.idleT = pose.amp < 0.05f ? pose.idleT + dt : 0.0f;
        pose.velY = velY;
        if (airborne) {
            pose.airT = pose.airT < 0.0f ? 0.0f : pose.airT + dt;
            pose.landT = -1.0f;
        } else {
            if (prevAir) pose.landT = 0.0f;
            else if (pose.landT >= 0.0f) {
                pose.landT += dt;
                if (pose.landT > LAND_RECOVER_S) pose.landT = -1.0f;
            }
            pose.airT = -1.0f;
        }
        prevAir = airborne;
    }

    glm::vec3 velocity(const glm::vec3& pos, float dt) const {
        if (!prevValid) return glm::vec3(0.0f);
        return (pos - prevPos) / (dt > 1e-4f ? dt : 1e-4f);
    }
};

// Whole-body offsets for one frame of the current clip blend, in the figure's
// local frame (+Z forward, +X left, metres and radians).
struct AnimLayer {
    glm::vec3 pelvis{0.0f};
    float torsoPitch = 0.0f, torsoRoll = 0.0f, torsoYaw = 0.0f;
    float headPitch = 0.0f, headYaw = 0.0f;
    glm::vec3 gun{0.0f};
    float stride = 0.38f, lift = 0.16f;
    float legTuck = 0.0f;
};

inline AnimLayer evalAnim(const PoseAnim& a, float crouch) {
    AnimLayer L;
    const float ph = a.phase, amp = a.amp, run = a.run;
    float breathe = sinf(a.idleT * 1.6f);
    float shift   = sinf(a.idleT * 0.45f);
    float idle    = (1.0f - amp) * fminf(1.0f, a.idleT * 2.0f);
    L.pelvis.y   += idle * breathe * 0.006f;
    L.pelvis.x   += idle * shift * 0.025f;
    L.torsoRoll  += idle * shift * 0.035f;
    L.torsoPitch += idle * breathe * 0.012f;
    L.headYaw    += idle * sinf(a.idleT * 0.31f + 1.0f) * 0.08f;
    L.gun.y      += idle * breathe * 0.004f;

    float bob2 = sinf(ph * 2.0f);
    float sway = sinf(ph);
    L.pelvis.y   += amp * (-0.012f - 0.022f * run) * (0.5f - 0.5f * bob2) - run * 0.05f;
    L.pelvis.x   += amp * sway * glm::mix(0.018f, 0.008f, run) * (1.0f + crouch * 0.8f);
    L.torsoYaw   += amp * sway * glm::mix(0.06f, 0.11f, run);
    L.torsoRoll  += amp * sway * 0.03f;
    L.torsoPitch += amp * glm::mix(0.05f, 0.22f, run) + crouch * 0.25f;
    L.headPitch  -= amp * glm::mix(0.03f, 0.14f, run) + crouch * 0.12f;
    L.gun        += amp * glm::vec3(sway * 0.012f, bob2 * glm::mix(0.015f, 0.035f, run),
                                    sinf(ph * 2.0f + 0.8f) * run * 0.03f);
    L.stride = glm::mix(glm::mix(0.38f, 0.18f, crouch), 0.46f, run) * amp;
    L.lift   = glm::mix(glm::mix(0.16f, 0.07f, crouch), 0.30f, run) * amp;

    if (a.airT >= 0.0f) {
        float t = fminf(1.0f, a.airT * 5.0f);
        float rising = glm::clamp(a.velY / JUMP_SPEED, -1.0f, 1.0f);
        L.legTuck    = t * (0.55f + 0.25f * fmaxf(0.0f, rising));
        L.torsoPitch += t * 0.12f;
        L.gun.y      += t * 0.04f;
        L.pelvis.y   -= t * 0.04f;
    }
    if (a.landT >= 0.0f) {
        float t = a.landT / LAND_RECOVER_S;
        float dip = sinf(t * 3.14159265f);
        L.pelvis.y   -= dip * 0.11f;
        L.torsoPitch += dip * 0.18f;
        L.gun.y      -= dip * 0.03f;
    }
    return L;
}
