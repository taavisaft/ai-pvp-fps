#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "shader.h"
#include "gl_loader.h"

struct Renderer;  // borrows lighting palette + shadow map; full type in grass.cpp

// Instanced 3D grass: crossed alpha-cutout quad clumps scattered in a ring around
// the camera. Placement is a deterministic hash grid — every ~0.64 m cell within
// GRASS_RADIUS rolls one or two small clumps (position jitter, yaw, scale) from hashes,
// so nothing is stored or synced; the instance buffer just rebuilds whenever the
// camera crosses a rebuild cell. Beyond the ring the ground's fragment-shader grass
// texture takes over (the shader dither-fades the handoff). Client-only cosmetic:
// no collision, no netcode, no shadow casting (receive only).
//
// Per-instance data is 8 floats: base pos.xyz, yaw, scale, terrain normal.xyz.
struct Grass {
    Shader shader;
    GLuint vao = 0, vbo = 0, ebo = 0, instVBO = 0;
    GLuint atlasTex = 0;                 // baked 4x1 thin-blade atlas
    bool   atlasIsAsset = false;         // retained for shader's optional 4x4 layout
    GLint  locAtlas4x4 = -1;             // extra uniform beyond Shader's cache
    GLsizei indexCount = 0;              // clump mesh (4 intersecting quads)
    GLint   instCap = 0;                 // GPU instance-buffer capacity (floats)
    std::vector<float> inst;             // rebuild scratch, 8 floats per clump
    int   count     = 0;                 // live instances
    float lastX = 1e9f, lastZ = 1e9f;    // camera pos at last rebuild

    bool init();                         // shaders + clump mesh + atlas bake
    void clear();                        // invalidate ring (map switch)
    // Draw after the opaque world, before water/HUD. Rebuilds the ring on demand.
    void draw(const Renderer& r, const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& eye, float time);
    void destroy();

  private:
    void rebuild(const glm::vec3& eye);
};
