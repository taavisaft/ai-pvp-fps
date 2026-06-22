#include "texture.h"
#include "map.h"
#include "stb_image.h"   // implementation lives in heightmap.cpp (shared by client+server)
#include <cmath>
#include <cstdlib>

GLuint loadTexture(const char* path) {
    stbi_set_flip_vertically_on_load(1);          // GL texture origin is bottom-left
    int w = 0, h = 0, c = 0;
    unsigned char* px = stbi_load(path, &w, &h, &c, 3);  // force RGB
    if (!px) return 0;                            // missing/unreadable -> caller falls back
    GLuint tex = uploadTextureRGB(px, w, h);
    stbi_image_free(px);
    return tex;
}

static float fractf(float x) { return x - floorf(x); }

static float smoothstepf(float a, float b, float x) {
    float t = (x - a) / (b - a);
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

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
    // Anisotropic filtering: keeps tiled ground sharp at grazing angles instead of
    // smearing into mip blur. EXT_texture_filter_anisotropic (GL 4.1 on macOS).
    #ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
    #define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
    #endif
    float maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    if (maxAniso > 8.0f) maxAniso = 8.0f;
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

GLuint makeMapTexture(const Box* boxes, int count, float worldHalf) {
    const int S = 512;
    unsigned char* px = (unsigned char*)malloc((size_t)S * S * 3);
    if (!px) return 0;

    // base terrain: muted khaki/green recon tone with low-freq fbm variation
    for (int y = 0; y < S; y++) {
        for (int x = 0; x < S; x++) {
            float wx = (((x + 0.5f) / S) * 2.0f - 1.0f) * worldHalf;
            float wz = (((y + 0.5f) / S) * 2.0f - 1.0f) * worldHalf;
            float n = 0.0f, amp = 0.5f, freq = 0.04f;
            for (int o = 0; o < 4; o++) {
                n += noise2(wx * freq + 13.0f, wz * freq + 7.0f) * amp;
                amp *= 0.5f; freq *= 2.0f;
            }
            float g = 0.30f + 0.14f * n;
            int i = (y * S + x) * 3;
            px[i + 0] = (unsigned char)(clamp01(g * 0.72f) * 255.0f);
            px[i + 1] = (unsigned char)(clamp01(g * 0.92f) * 255.0f);
            px[i + 2] = (unsigned char)(clamp01(g * 0.55f) * 255.0f);
        }
    }

    auto toPx = [&](float w) { return (int)(((w / worldHalf) * 0.5f + 0.5f) * S); };
    auto fillRect = [&](int x0, int x1, int z0, int z1, float r, float g, float b) {
        if (x0 < 0) x0 = 0; if (z0 < 0) z0 = 0;
        if (x1 > S) x1 = S; if (z1 > S) z1 = S;
        for (int y = z0; y < z1; y++)
            for (int x = x0; x < x1; x++) {
                int i = (y * S + x) * 3;
                px[i + 0] = (unsigned char)(clamp01(r) * 255.0f);
                px[i + 1] = (unsigned char)(clamp01(g) * 255.0f);
                px[i + 2] = (unsigned char)(clamp01(b) * 255.0f);
            }
    };

    int shadow = (int)(S * 0.012f);   // drop-shadow offset (toward +x,+z) in pixels
    // shadows first so roofs sit on top
    for (int k = 0; k < count; k++) {
        const Box& b = boxes[k];
        int x0 = toPx(b.center.x - b.half.x) + shadow, x1 = toPx(b.center.x + b.half.x) + shadow;
        int z0 = toPx(b.center.z - b.half.z) + shadow, z1 = toPx(b.center.z + b.half.z) + shadow;
        fillRect(x0, x1, z0, z1, 0.07f, 0.08f, 0.06f);
    }
    for (int k = 0; k < count; k++) {
        const Box& b = boxes[k];
        int x0 = toPx(b.center.x - b.half.x), x1 = toPx(b.center.x + b.half.x);
        int z0 = toPx(b.center.z - b.half.z), z1 = toPx(b.center.z + b.half.z);
        float top  = b.center.y + b.half.y;                 // roof height
        float lum  = 0.40f + 0.07f * (top < 5.0f ? top : 5.0f);  // taller = lighter concrete
        fillRect(x0, x1, z0, z1, lum * 1.02f, lum, lum * 0.96f);
        // 2px darker outline for definition
        fillRect(x0, x1, z0, z0 + 2, lum * 0.6f, lum * 0.6f, lum * 0.6f);
        fillRect(x0, x1, z1 - 2, z1, lum * 0.6f, lum * 0.6f, lum * 0.6f);
        fillRect(x0, x0 + 2, z0, z1, lum * 0.6f, lum * 0.6f, lum * 0.6f);
        fillRect(x1 - 2, x1, z0, z1, lum * 0.6f, lum * 0.6f, lum * 0.6f);
    }

    GLuint t = uploadTextureRGB(px, S, S);
    free(px);
    return t;
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
        // Corrugated industrial steel: ridges + fine brushing + grime + sparse scratches.
        float corr    = sinf(v * 6.2831853f * 4.0f) * 0.10f;            // sheet-metal ridges
        float brush   = (noise2(u * 220.0f, v * 5.0f) - 0.5f) * 0.07f;  // fine anisotropic brushing
        float grime   = (n - 0.5f) * 0.12f;                            // broad tonal variation
        float sc      = noise2(u * 70.0f + 3.1f, v * 70.0f);
        float scratch = sc > 0.85f ? 0.12f : 0.0f;                     // sparse bright scratches
        float base = 0.46f + corr + brush + grime + scratch;
        if (base < 0.12f) base = 0.12f;
        r = base * 0.94f; g = base * 0.98f; b = base * 1.07f;          // cool steel tint
    });
}

GLuint makeWoodTexture() {
    return makeTex([](float u, float v, float n, float& r, float& g, float& b) {
        // Stacked planks with grain running ALONG each plank (horizontal), dark gaps.
        float plankH = 0.25f;                                    // ~4 planks per tile
        float pidx   = floorf(v / plankH);
        float vin    = fractf(v / plankH);                       // 0..1 across a plank
        float ptone  = (hash2(pidx, 7.0f) - 0.5f) * 0.14f;       // each plank a bit different
        float wave   = noise2(u * 2.5f, pidx * 4.0f) * 3.0f;     // gentle grain waviness
        float grain  = sinf(vin * 11.0f + wave) * 0.5f + 0.5f;
        grain = powf(grain, 1.7f);                               // sharpen the dark grain lines
        float fiber  = (noise2(u * 60.0f, v * 6.0f) - 0.5f) * 0.08f;  // lengthwise fibers
        float seam   = smoothstepf(0.0f, 0.05f, fminf(vin, 1.0f - vin)); // 0 in the plank gap
        float light  = 0.70f + ptone - grain * 0.22f + fiber + (n - 0.5f) * 0.05f;
        light *= 0.45f + 0.55f * seam;                           // darken the gaps
        if (light < 0.12f) light = 0.12f;
        r = light * 0.60f; g = light * 0.40f; b = light * 0.22f; // warm brown
    });
}

GLuint makeRockTexture() {
    return makeTex([](float, float, float n, float& r, float& g, float& b) {
        float g0 = 0.35f + n * 0.22f;
        r = g0 * 1.05f; g = g0 * 0.95f; b = g0 * 0.88f;
    });
}

GLuint makeDirtTexture() {
    return makeTex([](float u, float v, float n, float& r, float& g, float& b) {
        // Warm brown soil: broad tonal variation + sparse darker grit specks.
        float base  = 0.30f + n * 0.18f;
        float speck = noise2(u * 90.0f, v * 90.0f) > 0.82f ? -0.07f : 0.0f;
        float t = base + speck;
        if (t < 0.10f) t = 0.10f;
        r = t * 0.66f; g = t * 0.47f; b = t * 0.30f;
    });
}

void destroyTexture(GLuint tex) {
    if (tex) glDeleteTextures(1, &tex);
}
