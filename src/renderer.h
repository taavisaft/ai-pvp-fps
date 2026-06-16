#pragma once
#include <SDL.h>
#include <glm/glm.hpp>
#include "shader.h"
#include "mesh.h"
#include "font.h"
#include "material.h"

struct Renderer {
    SDL_Window*   window    = nullptr;
    SDL_GLContext glContext = nullptr;
    int           width     = 1280;
    int           height    = 720;
    bool          wireframe = false;
    float         frameTime = 0.0f;   // seconds; grass wind shimmer (and a future
                                      // 3D-blade instanced pass) read the same clock

    Shader shader;
    Mesh   cube;     // unit cube, scaled per draw
    Mesh   ground;   // 100x100 quad at y=0
    Mesh   quad2d;   // unit quad for HUD rects
    Font   font;     // bitmap text, HUD pass only
    MaterialLib materials;

    bool  init(const char* title, int w, int h);
    float aspect() const;
    void  setTime(float t) { frameTime = t; }
    void  beginFrame(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye);
    void  drawCube(const glm::vec3& center, const glm::vec3& scale, const glm::vec3& color);
    void  drawCube(const glm::vec3& center, const glm::vec3& scale, MaterialId mat);
    void  drawCubeModel(const glm::mat4& model, const glm::vec3& color);  // oriented (gun)
    void  setView(const glm::mat4& view);   // override view uniform (mirror reflection pass)
    void  fillDepthFar();                   // far-plane quad; resets depth in stencil region
    void  drawGround();
    void  drawGround(MaterialId mat);
    // HUD pass: depth off, blending on, coordinates in NDC [-1,1]
    void  beginHUD();
    void  drawRect(const glm::vec2& center, const glm::vec2& size,
                   const glm::vec3& color, float alpha);
    // Like drawRect but rotated by angle (radians) about screen +Z. size is in
    // aspect-corrected square space so a 45 deg stroke renders at 45 deg.
    void  drawRectRot(const glm::vec2& center, const glm::vec2& size,
                      const glm::vec3& color, float alpha, float angle);
    // x,y = bottom-left in NDC, h = char height in NDC
    void  drawText(const char* s, float x, float y, float h,
                   const glm::vec3& color, float alpha);
    float textWidth(const char* s, float h) const;
    void  endHUD();
    void  endFrame();
    void  toggleWireframe();
    void  shutdown();
};
