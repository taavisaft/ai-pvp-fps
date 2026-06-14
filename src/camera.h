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

    void      addLook(float xrel, float yrel);  // mouse deltas, sensitivity 0.1
    float     aimYaw() const;                   // yaw + recoil
    float     aimPitch() const;                 // pitch + recoil, clamped
    void      applyRecoil(float dPitch, float dYaw);
    void      recoverRecoil(float dt, bool firingRecently);
    glm::vec3 front() const;                    // full 3D look direction (uses aim)
    glm::mat4 view() const;
    glm::mat4 proj(float aspect) const;
};
