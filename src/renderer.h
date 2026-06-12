#pragma once
#include <SDL.h>
#include <glm/glm.hpp>
#include "shader.h"
#include "mesh.h"
#include "font.h"

struct Renderer {
    SDL_Window*   window    = nullptr;
    SDL_GLContext glContext = nullptr;
    int           width     = 1280;
    int           height    = 720;
    bool          wireframe = false;

    Shader shader;
    Mesh   cube;     // unit cube, scaled per draw
    Mesh   ground;   // 100x100 quad at y=0
    Mesh   quad2d;   // unit quad for HUD rects
    Font   font;     // bitmap text, HUD pass only

    bool  init(const char* title, int w, int h);
    float aspect() const;
    void  beginFrame(const glm::mat4& view, const glm::mat4& proj);
    void  drawCube(const glm::vec3& center, const glm::vec3& scale, const glm::vec3& color);
    void  drawGround();
    // HUD pass: depth off, blending on, coordinates in NDC [-1,1]
    void  beginHUD();
    void  drawRect(const glm::vec2& center, const glm::vec2& size,
                   const glm::vec3& color, float alpha);
    // x,y = bottom-left in NDC, h = char height in NDC
    void  drawText(const char* s, float x, float y, float h,
                   const glm::vec3& color, float alpha);
    float textWidth(const char* s, float h) const;
    void  endHUD();
    void  endFrame();
    void  toggleWireframe();
    void  shutdown();
};
