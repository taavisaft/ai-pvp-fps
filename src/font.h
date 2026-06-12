#pragma once
#include <glm/glm.hpp>
#include "shader.h"

// Bitmap text: embedded 5x7 font baked into a GL texture atlas at init,
// drawn as textured quads in NDC. Use inside the renderer's HUD pass
// (blending on, depth off). Uppercase only; lowercase input is upcased.
struct Font {
    GLuint tex = 0, vao = 0, vbo = 0;
    Shader shader;

    bool  init();
    // x,y = bottom-left of first char in NDC; h = char height in NDC;
    // invAspect keeps glyphs square on non-square viewports.
    void  draw(const char* s, float x, float y, float h, float invAspect,
               const glm::vec3& color, float alpha);
    float width(const char* s, float h, float invAspect) const;
    void  destroy();
};
