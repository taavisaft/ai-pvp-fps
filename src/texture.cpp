#include "texture.h"
#include <cmath>
#include <cstdlib>

static float fractf(float x) { return x - floorf(x); }

static float hash2(float x, float y) {
    return fractf(sinf(x * 127.1f + y * 311.7f) * 43758.5453f);
}

static float noise2(float x, float y) {
    float ix = floorf(x), iy = floorf(y);
    float fx = x - ix, fy = y - iy;
    float a = hash2(ix, iy);
    float b = hash2(ix + 1.0f, iy);
    float c = hash2(ix, iy + 1.0f);
    float d = hash2(ix + 1.0f, iy + 1.0f);
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}

static void fbmTile(unsigned char* px, int w, int h,
                    void (*rgb)(float u, float v, float n, float& r, float& g, float& b)) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float u = (float)x / (float)w;
            float v = (float)y / (float)h;
            float n = 0.0f, amp = 0.5f, freq = 4.0f;
            for (int o = 0; o < 4; o++) {
                n += noise2(u * freq, v * freq) * amp;
                amp *= 0.5f;
                freq *= 2.0f;
            }
            float r, g, bch;
            rgb(u, v, n, r, g, bch);
            int i = (y * w + x) * 3;
            px[i + 0] = (unsigned char)(r * 255.0f);
            px[i + 1] = (unsigned char)(g * 255.0f);
            px[i + 2] = (unsigned char)(bch * 255.0f);
        }
    }
}

GLuint uploadTextureRGB(const unsigned char* px, int w, int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static GLuint makeTex(void (*rgb)(float, float, float, float&, float&, float&)) {
    constexpr int S = 256;
    unsigned char* px = (unsigned char*)malloc((size_t)S * S * 3);
    if (!px) return 0;
    fbmTile(px, S, S, rgb);
    GLuint t = uploadTextureRGB(px, S, S);
    free(px);
    return t;
}

GLuint makeGroundTexture() {
    return makeTex([](float, float, float n, float& r, float& g, float& b) {
        float g0 = 0.32f + n * 0.12f;
        r = g0 * 0.75f; g = g0; b = g0 * 0.55f;
    });
}

GLuint makeConcreteTexture() {
    return makeTex([](float u, float v, float n, float& r, float& g, float& b) {
        float g0 = 0.42f + n * 0.18f;
        float streak = sinf(v * 40.0f) * 0.02f;
        g0 += streak;
        r = g0; g = g0 * 0.98f; b = g0 * 0.95f;
        (void)u;
    });
}

GLuint makeMetalTexture() {
    return makeTex([](float u, float v, float n, float& r, float& g, float& b) {
        float scratch = sinf(v * 80.0f + n * 6.0f) * 0.04f;
        float g0 = 0.38f + n * 0.08f + scratch;
        r = g0; g = g0 * 1.02f; b = g0 * 1.05f;
        (void)u;
    });
}

GLuint makeWoodTexture() {
    return makeTex([](float u, float v, float n, float& r, float& g, float& b) {
        float ring = sinf(v * 28.0f + n * 3.0f) * 0.08f;
        r = 0.45f + ring + n * 0.06f;
        g = 0.30f + ring * 0.6f + n * 0.04f;
        b = 0.18f + n * 0.03f;
        (void)u;
    });
}

GLuint makeRockTexture() {
    return makeTex([](float, float, float n, float& r, float& g, float& b) {
        float g0 = 0.35f + n * 0.22f;
        r = g0 * 1.05f; g = g0 * 0.95f; b = g0 * 0.88f;
    });
}

void destroyTexture(GLuint tex) {
    if (tex) glDeleteTextures(1, &tex);
}
