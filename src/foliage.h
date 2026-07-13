#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "shader.h"
#include "gl_loader.h"
#include "spatial.h"

struct Renderer;  // borrows lighting palette + shadow map; full type in foliage.cpp

// Instanced vegetation for Paldiski/lobby. A six-triangle three-card profile samples the
// shared vegetation atlas and is drawn via glDrawElementsInstanced. Trees are scattered
// deterministically on the shared heightfield (slope-filtered) and CPU frustum-culled
// each frame so only the visible subset is uploaded + drawn. Pure decoration:
// client-only, no collision, no net sync.
//
// Per-instance data is 6 floats: world pos.xyz, yaw, scale, atlas tile.
struct TreeInstance { glm::vec3 pos; float yaw; float scale; float tile; };

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
    Shader depthShader;                  // instanced shadow-caster pass (alpha cutout)
    Shader blobShader;                   // far blob decals (beyond real-shadow range)
    GLuint vao = 0, vbo = 0, ebo = 0;   // combined tree geometry
    GLuint instVBO = 0;                  // per-instance transforms (dynamic)
    GLuint blobVao = 0, blobVbo = 0;     // ground decal quad (reuses instVBO)
    GLint  instCap  = 0;
    GLint  locCutoff = -1;               // extra uniform beyond Shader's cache
    GLint  locCutoffDepth = -1;
    GLint  locBlobRadius = -1, locBlobGround = -1;

    bool   loaded = false;              // false if tree.glb missing -> no-op
    std::vector<TreePart>     parts;    // sub-meshes (opaque trunk + cutout leaves)
    std::vector<GLuint>       texes;    // owned textures (for cleanup)
    std::vector<TreeInstance> trees;    // every scattered tree
    std::vector<float>        packed;   // visible subset, 6 floats each (upload scratch)
    SpatialGrid               grid;     // rejects whole vegetation chunks per frame
    float treeHeight = 8.0f;            // baked height at scale 1 (cull sphere + radius)
    float treeRadius = 4.0f;            // baked canopy radius at scale 1
    float sink = 1.05f;                 // absorb atlas padding and uneven sloped terrain

    bool init();                 // shaders + load tree.glb + instance buffer
    void generate(int maxTrees); // scatter trees in forest clumps on the heightfield
    void clear();                // drop all trees (e.g. leaving the field map)
    bool empty() const { return trees.empty(); }
    void draw(const Renderer& r, const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& eye, float time);
    // Shadow-caster pass: cull to the sun's frustum and write instanced depth with
    // leaf alpha cutout. Call between Renderer::beginShadowPass/endShadowPass.
    void drawDepth(const glm::mat4& lightSpace, float time);
    void destroy();

  private:
    // Frustum-cull all trees against `vp`, upload the visible subset to instVBO,
    // return the visible instance count. Shared by the main and shadow passes.
    int cullUpload(const glm::mat4& vp, const glm::vec3* eye = nullptr,
                   bool woodyOnly = false);
};
