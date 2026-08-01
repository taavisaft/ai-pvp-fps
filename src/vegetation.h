#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <climits>
#include "gl_loader.h"
#include "shader.h"
#include "frustum.h"
#include "spatial.h"

struct Renderer;

// Instanced vegetation for the taiga map: grass blades near the camera and a
// spruce forest at three LODs. Geometry is generated; the spruce branches carry
// one photo texture (textures/spruce_branch.png, CC0 needle spray, alpha
// cutout). Placement is deterministic from the shared terrain noise,
// client-side only.
//
// Grass: real blade geometry lives in camera-centered 16 m tiles out to ~90 m.
// Each blade sinks smoothly into the ground approaching that range (per-blade
// jitter staggers the edge), where the terrain shader's procedural grass color
// takes over — same greens, so the handoff is invisible. Tiles are stored in a
// toroidally-addressed slot grid and rebuilt (budgeted) as the camera crosses
// tile boundaries; by the time a tile enters the visible range its blades are
// still at zero height, so rebuilds never pop.
//
// Trees: one deterministic scatter (pineForestBiome patches + lone stragglers),
// culled per frame through a SpatialGrid, then bucketed by camera distance:
//   LOD0 detailed mesh (0..72 m) -> LOD1 low mesh (58..320 m) -> impostor quad
//   (280 m..map edge; the spruce baked to a texture once at startup).
// Adjacent LODs overlap across a fade band and draw with complementary
// screen-door dither (see veg.frag) — every tree is visible at every distance
// and never pops. LOD0 trees also render into the sun shadow map.
struct Vegetation {
    // 3D grass is parked (user judged the blade look not worth it yet) — the tile
    // machinery stays; flip this to bring it back once the tree pass looks right.
    static constexpr bool  GRASS_ENABLED = false;
    static constexpr float GRASS_TILE   = 16.0f;
    static constexpr int   GRASS_RING   = 6;                    // tiles each side
    static constexpr int   GRASS_SLOTS  = 2 * GRASS_RING + 1;   // 13x13 slot pool
    static constexpr float GRASS_RANGE  = 90.0f;                // blades exist to here
    static constexpr float GRASS_PER_M2 = 5.5f;   // tuft candidates (x3 blades each)

    static constexpr float TREE_FADE0 = 58.0f,  TREE_L0_END = 72.0f;
    static constexpr float TREE_FADE1 = 280.0f, TREE_L1_END = 320.0f;
    // Impostor far cap: past ~1.35 km a spruce billboard is a fog-dimmed speck, and
    // on a ridge the whole 2 km map's trees would otherwise queue as impostors every
    // frame (CPU bucket + stream re-upload + tens of thousands of alpha quads). Cap
    // and dither out over a band so nothing pops. Tunable — lower for more headroom.
    static constexpr float TREE_IMP_FADE = 1350.0f, TREE_IMP_END = 1500.0f;
    static constexpr float TREE_SHADOW_RANGE = 85.0f;   // LOD0 casters around camera
    // Bushes: one mesh, dithered fully out by BUSH_END (no impostor — undergrowth
    // is concealment detail, not silhouette; past 150 m it wouldn't cover a pixel).
    static constexpr float BUSH_FADE = 120.0f, BUSH_END = 150.0f;
    static constexpr float BUSH_SHADOW_RANGE = 40.0f;

    struct Tree { glm::vec3 pos; float scale, yaw, tint; };
    struct GrassTile {
        GLuint vao = 0, vbo = 0;
        int    count = 0;
        int    tx = INT_MIN, tz = INT_MIN;   // world tile held (INT_MIN = stale)
        float  minY = 0.0f, maxY = 0.0f;
    };

    Shader vegSh;        // instanced mesh vegetation (grass + tree LOD0/LOD1)
    Shader impSh;        // far-tree billboard impostors
    Shader vegDepthSh;   // instanced sun-depth pass (tree LOD0 shadow casters)
    // Custom uniforms not covered by Shader's standard cache:
    GLint locWind = -1, locRange = -1, locFadeIn = -1, locFadeOut = -1, locBake = -1;
    GLint locWindD = -1;                       // vegDepthSh windAmp
    GLint locImpSize = -1, locImpFadeIn = -1, locImpFadeOut = -1;  // impSh

    // Geometry: shared vertex/index buffers; one VAO per (mesh, instance stream).
    GLuint  bladeVbo = 0, bladeEbo = 0; GLsizei bladeIdx = 0;
    GLuint  l0Vbo = 0, l0Ebo = 0;       GLsizei l0Idx = 0;
    GLuint  l1Vbo = 0, l1Ebo = 0;       GLsizei l1Idx = 0;
    GLuint  bushVbo = 0, bushEbo = 0;   GLsizei bushIdx = 0;
    GLuint  quadVbo = 0;                          // impostor corners+uv (4 verts)
    GLuint  streamL0 = 0, streamL1 = 0, streamImp = 0, streamShadow = 0;
    GLuint  streamBush = 0, streamBushShadow = 0;
    GLuint  vaoL0 = 0, vaoL1 = 0, vaoImp = 0, vaoShadow = 0;
    GLuint  vaoBush = 0, vaoBushShadow = 0;
    GLuint  impTex = 0;                           // baked spruce atlas
    GLuint  branchTex = 0;                        // needle-spray photo, alpha cutout
    GLuint  bushTex = 0;                          // berry-bush photo, alpha cutout
    glm::vec2 impSize = {0.52f, 1.10f};           // world size of the bake, scale 1

    std::vector<Tree> trees;
    std::vector<Tree> bushes;                     // same instance layout as trees
    SpatialGrid       grid;
    SpatialGrid       bushGrid;
    bool              placed = false;
    GrassTile         tiles[GRASS_SLOTS][GRASS_SLOTS];
    // Instance staging, reused every frame (capacity settles, no steady-state allocs)
    std::vector<float> bufL0, bufL1, bufImp, bufShadow, bufTile;
    std::vector<float> bufBush, bufBushShadow;

    bool init(const char* basePath);   // shaders + meshes + impostor bake (GL ready)
    void invalidate();                 // active map changed: drop placements/tiles
    void drawLit(const Renderer& r, const Frustum& fr, const glm::vec3& eye);
    void drawShadow(const Frustum& sunFr, const glm::vec3& focus, float time,
                    const glm::mat4& lightSpace);
    void destroy();

    void buildTrees();
    void buildBushes();
    void rebuildTile(GrassTile& t, int tx, int tz);
};

// veg_mesh.cpp — build-time helpers.
void   vegBuildBlade(std::vector<float>& v, std::vector<unsigned>& idx);
void   vegBuildSpruce(std::vector<float>& v, std::vector<unsigned>& idx, bool low);
void   vegBuildBush(std::vector<float>& v, std::vector<unsigned>& idx);
GLuint vegMakeVAO(GLuint vbo, GLuint ebo, GLuint inst);   // 10-float verts + stream
bool   vegBakeImpostor(Vegetation& veg, int texW, int texH);
// GLSL-mirror of basic.frag's fbm (same float math) so CPU placement masks agree
// with the shader's dirt-field / dry-patch regions.
float  vegFbm(float x, float y);
