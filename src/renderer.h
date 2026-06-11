#pragma once
#include <SDL.h>
#include <glm/glm.hpp>
#include "shader.h"
#include "mesh.h"

struct Renderer {
    SDL_Window*   window    = nullptr;
    SDL_GLContext glContext = nullptr;
    int           width     = 1280;
    int           height    = 720;
    bool          wireframe = false;

    Shader shader;
    Mesh   cube;     // unit cube, scaled per draw
    Mesh   ground;   // 100x100 quad at y=0

    bool  init(const char* title, int w, int h);
    float aspect() const;
    void  beginFrame(const glm::mat4& view, const glm::mat4& proj);
    void  drawCube(const glm::vec3& center, const glm::vec3& scale, const glm::vec3& color);
    void  drawGround();
    void  endFrame();
    void  toggleWireframe();
    void  shutdown();
};
