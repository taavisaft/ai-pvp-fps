#include "font.h"
#include <cstring>
#include <cstdio>
#include <SDL.h>

// Classic 5x7 font, one glyph per 5 bytes, column-major, bit 0 = top row.
// Covers ASCII 32..90 (space through 'Z'); enough for a numeric/uppercase HUD.
static const unsigned char FONT5X7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // !
    {0x00,0x07,0x00,0x07,0x00}, // "
    {0x14,0x7F,0x14,0x7F,0x14}, // #
    {0x24,0x2A,0x7F,0x2A,0x12}, // $
    {0x23,0x13,0x08,0x64,0x62}, // %
    {0x36,0x49,0x55,0x22,0x50}, // &
    {0x00,0x05,0x03,0x00,0x00}, // '
    {0x00,0x1C,0x22,0x41,0x00}, // (
    {0x00,0x41,0x22,0x1C,0x00}, // )
    {0x14,0x08,0x3E,0x08,0x14}, // *
    {0x08,0x08,0x3E,0x08,0x08}, // +
    {0x00,0x50,0x30,0x00,0x00}, // ,
    {0x08,0x08,0x08,0x08,0x08}, // -
    {0x00,0x60,0x60,0x00,0x00}, // .
    {0x20,0x10,0x08,0x04,0x02}, // /
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x00,0x56,0x36,0x00,0x00}, // ;
    {0x08,0x14,0x22,0x41,0x00}, // <
    {0x14,0x14,0x14,0x14,0x14}, // =
    {0x00,0x41,0x22,0x14,0x08}, // >
    {0x02,0x01,0x51,0x09,0x06}, // ?
    {0x32,0x49,0x79,0x41,0x3E}, // @
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
};
static constexpr int GLYPHS     = (int)(sizeof(FONT5X7) / sizeof(FONT5X7[0]));
static constexpr int CELL       = 8;                 // px per glyph cell (5 used + pad)
static constexpr int ATLAS_W    = GLYPHS * CELL;
static constexpr int ATLAS_H    = CELL;
static constexpr float ADVANCE  = 6.0f / CELL;       // glyph + 1 col gap, in cell widths
static constexpr int MAX_CHARS  = 256;               // per draw() call

static int glyphIndex(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c < 32 || c > 90) c = '?';
    return c - 32;
}

bool Font::init() {
    // Load shaders next to the executable (so Finder / any cwd works); fall back
    // to cwd for running from the source tree. Mirrors Renderer::init.
    const char* sdlBase = SDL_GetBasePath();
    char base[512], vpath[600], fpath[600];
    snprintf(base, sizeof(base), "%s", sdlBase ? sdlBase : "");
    snprintf(vpath, sizeof(vpath), "%sshaders/text.vert", base);
    snprintf(fpath, sizeof(fpath), "%sshaders/text.frag", base);
    if (!shader.load(vpath, fpath)) {
        if (!shader.load("shaders/text.vert", "shaders/text.frag")) return false;
    }

    static unsigned char pixels[ATLAS_W * ATLAS_H];
    memset(pixels, 0, sizeof(pixels));
    for (int g = 0; g < GLYPHS; g++)
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if (FONT5X7[g][col] & (1 << row))
                    pixels[row * ATLAS_W + g * CELL + col] = 255;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_W, ATLAS_H, 0,
                 GL_RED, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    if (!tex || !vao || !vbo) return false;
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_CHARS * 6 * 4 * sizeof(float),
                 nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

float Font::width(const char* s, float h, float invAspect) const {
    return (float)strlen(s) * h * invAspect * ADVANCE;
}

void Font::draw(const char* s, float x, float y, float h, float invAspect,
                const glm::vec3& color, float alpha) {
    static float verts[MAX_CHARS * 6 * 4];
    float w    = h * invAspect;        // square cell
    float step = w * ADVANCE;
    int   n    = 0;

    for (const char* c = s; *c && n < MAX_CHARS; c++, n++) {
        int   g  = glyphIndex(*c);
        float u0 = (float)(g * CELL) / ATLAS_W;
        float u1 = (float)(g * CELL + CELL) / ATLAS_W;
        float x0 = x + n * step, x1 = x0 + w;
        float y0 = y, y1 = y + h;
        // v=0 is the glyph's top row
        float q[6][4] = {
            {x0, y0, u0, 1}, {x1, y0, u1, 1}, {x1, y1, u1, 0},
            {x1, y1, u1, 0}, {x0, y1, u0, 0}, {x0, y0, u0, 1},
        };
        memcpy(&verts[n * 24], q, sizeof(q));
    }
    if (n == 0) return;

    shader.use();
    shader.setVec3(shader.locColor, color);
    shader.setFloat(shader.locAlpha, alpha);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // Previous text draws may still be reading this storage. Orphan it before
    // replacing the vertices so the driver needn't wait for those draws.
    glBufferData(GL_ARRAY_BUFFER, MAX_CHARS * 6 * 4 * sizeof(float),
                 nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, n * 24 * sizeof(float), verts);
    glDrawArrays(GL_TRIANGLES, 0, n * 6);
    glBindVertexArray(0);
}

void Font::destroy() {
    if (tex) glDeleteTextures(1, &tex);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    tex = vbo = vao = 0;
    shader.destroy();
}
