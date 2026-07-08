#pragma once
#include <glm/glm.hpp>

struct Camera {
    float     yaw   = -90.0f;  // degrees; -90 looks down -Z (player mouse aim)
    float     pitch = 0.0f;    // degrees, clamped [-89, 89] (player mouse aim)
    float     fov   = 75.0f;   // vertical FOV, narrowed when aiming
    glm::vec3 eye   = {0.0f, 1.7f, 0.0f};

    // Recoil offset added on top of the mouse aim; the sum is what's rendered and
    // reported to the server, so bullets follow the kicked barrel direction.
    float     recoilYaw   = 0.0f;
    float     recoilPitch = 0.0f;

    float     lean = 0.0f;        // smoothed lean -1 (left)..+1 (right), Q/E

    // Render-only third-person view. tpDist > 0 = active (flag only); tpPos is the
    // collision-resolved camera position (computed in main, where terrain + cover boxes
    // are known) that view() looks from. Aim math / muzzle origin stay at the head so
    // shooting is unchanged. A testing aid to watch your own body move as enemies see it.
    float     tpDist = 0.0f;
    glm::vec3 tpPos  = {0.0f, 0.0f, 0.0f};

    void      addLook(float xrel, float yrel);  // mouse deltas, sensitivity 0.1
    void      updateLean(float target, float dt); // lerp lean toward target (-1..1)
    float     aimYaw() const;                   // yaw + recoil
    float     aimPitch() const;                 // pitch + recoil, clamped
    void      applyRecoil(float dPitch, float dYaw);
    void      recoverRecoil(float dt, bool firingRecently);
    glm::vec3 front() const;                    // full 3D look direction (uses aim)
    glm::vec3 rightFlat() const;                // horizontal right of aim (lean axis)
    glm::vec3 eyePos() const;                   // eye shifted by current lean
    glm::vec3 up() const;                        // world up rolled by current lean
    glm::mat4 view() const;
    glm::mat4 proj(float aspect) const;
};
