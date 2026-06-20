#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "shader.h"
#include "gl_loader.h"

struct Renderer;  // borrows lighting palette + shadow map; full type in foliage.cpp

// Instanced tree field for MAP_FIELD. A textured glTF tree (models/tree.glb) is loaded
// once and drawn for every instance via glDrawElementsInstanced. Trees are scattered
// deterministically on the shared heightfield (slope-filtered) and CPU frustum-culled
// each frame so only the visible subset is uploaded + drawn. Pure decoration:
// client-only, no collision, no net sync.
//
// Per-instance data is 5 floats: world pos.xyz, yaw (radians), uniform scale.
struct TreeInstance { glm::vec3 pos; float yaw; float scale; };

// One draw range of the tree, sharing the combined VBO/EBO. Opaque parts and
// alpha-masked (cutout leaf) parts differ in cutoff and face culling.
struct TreePart {
    GLuint  tex         = 0;       // baseColor texture (RGBA)
    GLuint  indexOffset = 0;       // byte offset into the EBO
    GLsizei indexCount  = 0;
    float   alphaCutoff = -1.0f;   // <0 = opaque; else discard alpha < cutoff
    bool    doubleSided = false;   // leaves: draw both faces
};

struct Foliage {
    Shader shader;
    GLuint vao = 0, vbo = 0, ebo = 0;   // combined tree geometry
    GLuint instVBO = 0;                  // per-instance transforms (dynamic)
    GLint  instCap  = 0;
    GLint  locCutoff = -1;               // extra uniform beyond Shader's cache

    bool   loaded = false;              // false if tree.glb missing -> no-op
    std::vector<TreePart>     parts;    // sub-meshes (opaque trunk + cutout leaves)
    std::vector<GLuint>       texes;    // owned textures (for cleanup)
    std::vector<TreeInstance> trees;    // every scattered tree
    std::vector<float>        packed;   // visible subset, 5 floats each (upload scratch)
    float treeHeight = 8.0f;            // baked height at scale 1 (cull sphere + radius)
    float treeRadius = 4.0f;            // baked canopy radius at scale 1

    bool init();                 // shader + load tree.glb + instance buffer
    void generate(int count);    // scatter `count` trees on the field heightfield
    void clear();                // drop all trees (e.g. leaving the field map)
    bool empty() const { return trees.empty(); }
    void draw(const Renderer& r, const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& eye, float time);
    void destroy();
};
