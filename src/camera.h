#pragma once
#include <glm/glm.hpp>

struct Camera {
    float     yaw   = -90.0f;  // degrees; -90 looks down -Z
    float     pitch = 0.0f;    // degrees, clamped [-89, 89]
    glm::vec3 eye   = {0.0f, 1.7f, 0.0f};

    void      addLook(float xrel, float yrel);  // mouse deltas, sensitivity 0.1
    glm::vec3 front() const;                    // full 3D look direction
    glm::mat4 view() const;
    glm::mat4 proj(float aspect) const;
};
