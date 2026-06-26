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

    // Lazily-built per-map satellite textures (indexed by MapId 0..2).
    GLuint mapTex[3]     = {0, 0, 0};
    bool   mapTexTried[3] = {false, false, false};

    // Shadow map: single ortho sun frustum following the camera (near-field shadows).
    GLuint    shadowFBO   = 0;
    GLuint    shadowTex   = 0;
    int       shadowSize  = 2048;
    glm::mat4 lightSpace  = glm::mat4(1.0f);
    int       fbW = 0, fbH = 0;   // drawable size, to restore viewport after shadow pass
    // Daylight palette — single source of truth, pushed to both basic and sky programs.
    glm::vec3 sunDir        = glm::normalize(glm::vec3(0.50f, 0.65f, 0.25f));
    glm::vec3 skyZenith     = {0.30f, 0.50f, 0.78f};
    glm::vec3 skyHorizon    = {0.74f, 0.82f, 0.90f};
    glm::vec3 groundAmbient = {0.26f, 0.27f, 0.24f};
    Mesh   cube;     // unit cube, scaled per draw
    Mesh   ground;   // 100x100 quad at y=0
    Mesh   terrain;  // 1 km^2 heightfield (MAP_FIELD)
    Mesh   quad2d;   // unit quad for HUD rects
    Font   font;     // bitmap text, HUD pass only
    MaterialLib materials;
    Foliage foliage; // instanced trees (MAP_FIELD only)

    // City facade buildings (MAP_CITY): one textured wall mesh + concrete roof cap per
    // building, built once from gCityBuildings on the first city frame.
    struct CityDraw { Mesh wall; glm::vec3 center, capCenter, capScale; };
    std::vector<CityDraw> cityDraws;
    GLuint facadeTex = 0;
    bool   cityBuilt = false;

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
    void  setView(const glm::mat4& view);   // override view uniform (mirror reflection pass)
    void  fillDepthFar();                   // far-plane quad; resets depth in stencil region
    void  drawGround();
    void  drawGround(MaterialId mat);
    void  drawTerrain();   // heightfield ground for MAP_FIELD
    // City facade buildings: lazy-builds meshes on first call, draws walls (UV facade)
    // + roof caps via the active program, so it works in both the shadow and lit pass.
    void  drawCityWorld();
    void  buildCityMeshes();
    // Instanced tree field; scatters lazily on first field-map frame, culls per frame.
    void  drawFoliage(const glm::mat4& view, const glm::mat4& proj,
                      const glm::vec3& eye, float time);
    // Trees as shadow casters — call inside the shadow pass (uses lightSpace).
    void  drawFoliageDepth(float time);
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
