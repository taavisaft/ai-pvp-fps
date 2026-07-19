#pragma once
#include <SDL.h>
#include <vector>
#include <glm/glm.hpp>
#include "shader.h"
#include "mesh.h"
#include "font.h"
#include "material.h"
#include "foliage.h"
#include "building.h"
#include "props.h"
#include "grass.h"
#include "frustum.h"

struct Box;  // map.h; for the satellite map-texture bake

struct Renderer {
    SDL_Window*   window    = nullptr;
    SDL_GLContext glContext = nullptr;
    int           width     = 1280;
    int           height    = 720;
    bool          wireframe = false;
    float         frameTime = 0.0f;   // seconds; grass wind shimmer (and a future
                                      // 3D-blade instanced pass) read the same clock

    Shader shader;
    Shader skyShader;    // fullscreen gradient sky + sun disk
    Shader depthShader;  // sun-POV depth pass for shadow mapping
    Shader texShader;    // textured HUD quad (satellite minimap)
    GLint  texUvCenterLoc = -1, texUvHalfLoc = -1;  // texShader custom uniforms
    Shader* active = nullptr;  // program the world draws target (basic or depth)

    // Lazily-built per-map satellite textures (indexed by MapId).
    GLuint mapTex[MAP_COUNT]      = {};
    bool   mapTexTried[MAP_COUNT] = {};

    // Shadow map: single ortho sun frustum following the camera (near-field shadows).
    GLuint    shadowFBO   = 0;
    GLuint    shadowTex   = 0;
    int       shadowSize  = 2048;
    glm::mat4 lightSpace  = glm::mat4(1.0f);
    glm::vec3 shadowFocus = glm::vec3(0.0f);  // camera eye the sun frustum is centered on
    int       fbW = 0, fbH = 0;   // drawable size, to restore viewport after shadow pass
    // Daylight palette — single source of truth, pushed to both basic and sky programs.
    // All fields are set together by setAtmosphere(); defaults = ATMO_CLEAR.
    glm::vec3 sunDir        = glm::normalize(glm::vec3(0.50f, 0.65f, 0.25f));
    glm::vec3 sunColor      = {1.00f, 0.96f, 0.88f};
    glm::vec3 skyZenith     = {0.30f, 0.50f, 0.78f};
    glm::vec3 skyHorizon    = {0.74f, 0.82f, 0.90f};
    glm::vec3 groundAmbient = {0.26f, 0.27f, 0.24f};
    float fogDist      = 350.0f;  // distance-fog reach (m)
    float fogHeightAmt = 0.35f;   // low-altitude haze boost (valleys fill first)
    float cloudAmount  = 0.20f;   // drifting cloud-shadow strength 0..1
    float exposure     = 0.82f;   // pre-tonemap exposure
    float saturation   = 1.12f;   // grade saturation (1 = neutral)
    int   atmoPreset   = 0;       // ATMO_* currently applied

    // Atmosphere presets: DayZ overcast gloom / PUBG warm clear / golden-hour dusk.
    enum Atmo { ATMO_CLEAR = 0, ATMO_OVERCAST, ATMO_GOLDEN, ATMO_COUNT };
    void setAtmosphere(int preset);   // sets every palette field above
    Mesh   cube;     // unit cube, scaled per draw
    Mesh   ground;   // 100x100 quad at y=0 (water plane, scaled per draw)
    Mesh   terrain;  // Paldiski heightfield (built after setMap)
    Mesh   quad2d;   // unit quad for HUD rects
    Font   font;     // bitmap text, HUD pass only
    MaterialLib materials;
    Foliage foliage; // instanced trees
    Props   props;   // instanced street props
    Grass   grass;   // instanced 3D grass ring around the camera

    // Town facade buildings: one textured wall mesh + concrete roof cap per building,
    // built once from gTownBuildings on the first frame.
    struct TownDraw { Mesh wall, decals; glm::vec3 center, capCenter, capScale;
                      glm::vec3 cullCenter, cullHalf; };  // whole-building AABB for cull
    std::vector<TownDraw> townDraws;
    GLuint facadeTex = 0;
    GLuint decalTex  = 0;
    bool   townBuilt = false;
    int    worldBuiltFor = -1;   // MapId the lazy caches (town/trees/props) were built for

    bool  init(const char* title, int w, int h);
    float aspect() const;
    void  setTime(float t) { frameTime = t; }
    void  beginFrame(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye);
  void  drawSky(const glm::mat4& view, const glm::mat4& proj);  // call right after beginFrame
  // Shadow pass: render world geometry between these, viewed from the sun. Centers
  // the light frustum on `focus` (the camera eye). Sets the depth program active.
  void  beginShadowPass(const glm::vec3& focus);
  void  endShadowPass();
    void  drawCube(const glm::vec3& center, const glm::vec3& scale, const glm::vec3& color);
    void  drawCube(const glm::vec3& center, const glm::vec3& scale, MaterialId mat);
    void  drawCubeModel(const glm::mat4& model, const glm::vec3& color);  // oriented (gun)
    // Alpha-blended oriented cube for debug overlays (hitbox view). Depth-tested but
    // not depth-written; lit-pass only — never call during the shadow pass.
    void  drawCubeModelTranslucent(const glm::mat4& model, const glm::vec3& color,
                                   float alpha);
    void  drawTerrain();   // Paldiski heightfield ground (lobby: flat quad instead)
    void  drawGround();    // flat 2*gArenaHalf quad at y=0 (lobby pad)
    // Drop lazily-built world caches (town meshes, tree/prop scatters) when the
    // active map changed — call once per frame before the world passes.
    void  invalidateWorldOnMapChange();
    // Translucent sea plane at SEA_LEVEL, spanning past the map edge so the water
    // meets the fog. Draw late in the lit pass (after opaque world), never in shadow.
    void  drawWater();
    // Town facade buildings: lazy-builds meshes on first call, draws walls (UV facade)
    // + roof caps via the active program, so it works in both the shadow and lit pass.
    void  drawTownWorld(const Frustum& fr, const glm::vec3& eye);
    void  buildTownMeshes();
    // Instanced tree field; scatters lazily on first field-map frame, culls per frame.
    void  drawFoliage(const glm::mat4& view, const glm::mat4& proj,
                      const glm::vec3& eye, float time);
    // Trees as shadow casters — call inside the shadow pass (uses lightSpace).
    void  drawFoliageDepth(float time);
    // Instanced city props; scatters lazily on first city frame, culls per frame.
    void  drawProps(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye,
                    float time);
    void  drawPropsDepth();   // props as shadow casters (call inside the shadow pass)
    // 3D grass ring; draw after the opaque world, before water (no shadow casting).
    void  drawGrass(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye,
                    float time);
    // HUD pass: depth off, blending on, coordinates in NDC [-1,1]
    void  beginHUD();
    void  drawRect(const glm::vec2& center, const glm::vec2& size,
                   const glm::vec3& color, float alpha);
    // Like drawRect but rotated by angle (radians) about screen +Z. size is in
    // aspect-corrected square space so a 45 deg stroke renders at 45 deg.
    void  drawRectRot(const glm::vec2& center, const glm::vec2& size,
                      const glm::vec3& color, float alpha, float angle);
    // Textured HUD quad. Samples the sub-rect [uvCenter +/- uvHalf] of `tex`
    // (uvHalf {0.5,0.5} = whole image). Restores the flat shader after.
    void  drawTexQuad(const glm::vec2& center, const glm::vec2& size, unsigned int tex,
                      float alpha, const glm::vec2& uvCenter = {0.5f, 0.5f},
                      const glm::vec2& uvHalf = {0.5f, 0.5f},
                      const glm::vec3& tint = {1.0f, 1.0f, 1.0f});
    // Satellite texture for a map: hand-made textures/map_<name>.png if present,
    // else a procedural bake from `boxes`. Built once and cached. 0 on failure.
    unsigned int mapTexture(int mapId, const Box* boxes, int count, float worldHalf);
    // x,y = bottom-left in NDC, h = char height in NDC
    void  drawText(const char* s, float x, float y, float h,
                   const glm::vec3& color, float alpha);
    float textWidth(const char* s, float h) const;
    void  endHUD();
    void  endFrame();
    void  toggleWireframe();
    void  shutdown();
};
