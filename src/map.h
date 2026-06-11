#pragma once
#include <glm/glm.hpp>

// Static arena geometry, shared by server (collision) and client (collision + render).
// All boxes sit on the ground. Obstacle thickness stays >= 1.0 so bullets
// (max step ~0.85 m per physics tick) cannot tunnel through.

struct Box {
    glm::vec3 center;  // y = half.y (resting on ground)
    glm::vec3 half;
};

inline const Box MAP_BOXES[] = {
    // center pillar
    {{ 0.0f, 1.5f,  0.0f}, {1.0f, 1.5f, 1.0f}},
    // walls on the axes
    {{ 6.0f, 1.0f,  0.0f}, {0.6f, 1.0f, 2.5f}},
    {{-6.0f, 1.0f,  0.0f}, {0.6f, 1.0f, 2.5f}},
    {{ 0.0f, 1.0f,  6.0f}, {2.5f, 1.0f, 0.6f}},
    {{ 0.0f, 1.0f, -6.0f}, {2.5f, 1.0f, 0.6f}},
    // crates on the diagonals (low — can be shot over)
    {{ 4.0f, 0.8f,  4.0f}, {0.8f, 0.8f, 0.8f}},
    {{-4.0f, 0.8f,  4.0f}, {0.8f, 0.8f, 0.8f}},
    {{ 4.0f, 0.8f, -4.0f}, {0.8f, 0.8f, 0.8f}},
    {{-4.0f, 0.8f, -4.0f}, {0.8f, 0.8f, 0.8f}},
    // tall pillars on the outer diagonals
    {{ 9.0f, 1.5f,  9.0f}, {0.7f, 1.5f, 0.7f}},
    {{-9.0f, 1.5f,  9.0f}, {0.7f, 1.5f, 0.7f}},
    {{ 9.0f, 1.5f, -9.0f}, {0.7f, 1.5f, 0.7f}},
    {{-9.0f, 1.5f, -9.0f}, {0.7f, 1.5f, 0.7f}},
};
inline constexpr int MAP_BOX_COUNT = (int)(sizeof(MAP_BOXES) / sizeof(MAP_BOXES[0]));

inline constexpr float ARENA_HALF = 45.0f;  // players clamped to ±this on X/Z
