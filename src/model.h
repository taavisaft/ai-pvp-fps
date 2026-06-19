#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "gl_loader.h"

// A static mesh loaded from a .glb (glTF binary). Skinning/nodes ignored: the gun
// model's parts are already assembled in one shared space, so we render raw bind-pose
// geometry. No textures — each primitive is flat-colored by its material baseColor.
struct ModelPart {
    GLsizei   indexCount;
    GLuint    indexOffset;   // first index, in elements (not bytes)
    glm::vec3 color;
};

struct Model {
    GLuint vao = 0, vbo = 0, ebo = 0;
    std::vector<ModelPart> parts;
    glm::vec3 boundsMin{0.0f}, boundsMax{0.0f};

    bool loadGLB(const char* path);   // false if missing/unreadable (caller falls back)
    void destroy();

    glm::vec3 center() const { return (boundsMin + boundsMax) * 0.5f; }
    glm::vec3 size()   const { return boundsMax - boundsMin; }
};
