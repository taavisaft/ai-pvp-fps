#pragma once
#include <glm/glm.hpp>
#include "gl_loader.h"
#include "map.h"

// Shared world materials — one texture tiled on many surfaces (HL2/CS style).
enum MaterialId : uint8_t {
    MAT_GROUND = 0,
    MAT_CONCRETE,
    MAT_METAL,
    MAT_WOOD,
    MAT_ROCK,
    MAT_COUNT
};

struct Material {
    GLuint    tex  = 0;
    glm::vec3 tint = {1.0f, 1.0f, 1.0f};
    float     tile = 1.0f;   // world meters per texture repeat (triplanar)
    float     spec = 0.0f;   // cheap view specular (metal)
};

struct MaterialLib {
    Material mats[MAT_COUNT]{};

    bool init();                       // procedural tileable textures at startup
    void bind(MaterialId id) const;    // texture unit 0
    void destroy();
};

// Per-map-box material (parallel to MAP_BOXES; server ignores).
inline const MaterialId MAP_BOX_MAT[MAP_BOX_COUNT] = {
    MAT_CONCRETE, // center pillar
    MAT_CONCRETE, MAT_CONCRETE, MAT_CONCRETE, MAT_CONCRETE, // axis walls
    MAT_WOOD, MAT_WOOD, MAT_WOOD, MAT_WOOD,                 // crates
    MAT_CONCRETE, MAT_CONCRETE, MAT_CONCRETE, MAT_CONCRETE, // outer pillars
};
