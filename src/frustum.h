#pragma once
#include <glm/glm.hpp>

// View-frustum cull helper shared by every world pass (main view + sun shadow).
// Planes are extracted from a view-projection matrix via Gribb-Hartmann and
// normalized so the signed distance is in world units. Tests are conservative:
// a "visible" result may include objects just outside (never the reverse), which
// is the safe direction for culling.
struct Frustum {
    glm::vec4 planes[6];  // xyz = normal (points inward), w = offset

    // GLM is column-major: m[col][row]. Rows of vp combine into the 6 clip planes.
    static Frustum fromVP(const glm::mat4& vp) {
        glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
        glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
        glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
        glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);
        Frustum f;
        f.planes[0] = row3 + row0;  // left
        f.planes[1] = row3 - row0;  // right
        f.planes[2] = row3 + row1;  // bottom
        f.planes[3] = row3 - row1;  // top
        f.planes[4] = row3 + row2;  // near
        f.planes[5] = row3 - row2;  // far
        for (auto& p : f.planes) p /= glm::length(glm::vec3(p));
        return f;
    }

    // Bounding sphere fully on the outside of any plane -> culled.
    bool sphereVisible(const glm::vec3& c, float r) const {
        for (const auto& p : planes)
            if (glm::dot(glm::vec3(p), c) + p.w < -r) return false;
        return true;
    }

    // AABB given by center + half-extents. Uses the box's projection onto each
    // plane normal (the "negative vertex" radius) — exact for axis-aligned boxes.
    bool aabbVisible(const glm::vec3& c, const glm::vec3& half) const {
        for (const auto& p : planes) {
            glm::vec3 n(p);
            float r = glm::dot(half, glm::abs(n));   // projected half-size
            if (glm::dot(n, c) + p.w < -r) return false;
        }
        return true;
    }
};
