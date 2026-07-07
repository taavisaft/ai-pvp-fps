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
    MAT_DIRT,
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
    bool     groundHasImage = false;   // true if textures/ground.* loaded (vs procedural grass)

    bool init();                       // image textures if present, else procedural
    void bind(MaterialId id) const;    // texture unit 0
    void destroy();
};

// Map boxes carry a BoxSurface tag (generated); map it to a renderer material.
inline MaterialId surfaceMat(uint8_t surf) {
    switch (surf) {
        case SURF_METAL: return MAT_METAL;
        case SURF_WOOD:  return MAT_WOOD;
        default:         return MAT_CONCRETE;
    }
}

// Material for box i of the active map.
inline MaterialId mapBoxMaterial(int i) { return surfaceMat(gTownSurf[i]); }
