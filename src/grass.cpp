#include "grass.h"
#include "renderer.h"   // full Renderer type (lighting palette + shadow map)
#include "map.h"         // terrainHeight, gTownBoxes, mapHash/mapRand
#include "texture.h"     // uploadTextureRGB (procedural fallback path)
#include "stb_image.h"   // implementation lives in texture.cpp
#include <SDL.h>
#include <cstdio>
#include <cmath>

// --- tuning ------------------------------------------------------------------
namespace {
constexpr float GRASS_RADIUS   = 45.0f;  // clump ring around the camera (m)
constexpr float CELL           = 0.95f;  // one potential clump per cell (m)
constexpr float REBUILD_STEP   = 3.0f;   // camera travel that triggers a rebuild (m)
constexpr float MIN_GROUND_Y   = 1.35f;  // no grass on beach/sea (matches sand splat)
constexpr float MAX_SLOPE      = 0.55f;  // no grass on rocky steeps (matches trees)
constexpr float BASE_KEEP      = 0.88f;  // clump probability on ideal ground
constexpr int   ATLAS          = 256;    // atlas cell size (px), 4 columns

// Density thins with distance (fillrate) — most clumps live near the camera.
inline float keepAtDist(float d) {
    if (d < 18.0f) return BASE_KEEP;
    return BASE_KEEP * (1.0f - 0.55f * (d - 18.0f) / (GRASS_RADIUS - 18.0f));
}

// --- procedural blade atlas ---------------------------------------------------
// 4 columns of painted blade clusters (column 3 gets wildflower specks). RGBA,
// alpha 0 background — same no-asset ethos as the facade texture.
void paintAtlas(std::vector<unsigned char>& px) {
    const int W = ATLAS * 4, H = ATLAS;
    px.assign((size_t)W * H * 4, 0);
    auto put = [&](int x, int y, float r, float g, float b, float a) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        size_t o = ((size_t)y * W + x) * 4;
        px[o]     = (unsigned char)(r * 255.0f);
        px[o + 1] = (unsigned char)(g * 255.0f);
        px[o + 2] = (unsigned char)(b * 255.0f);
        px[o + 3] = (unsigned char)(a * 255.0f);
    };
    unsigned rng = 0x2545F491u;
    auto rnd = [&]() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float)(rng & 0xffffff) / (float)0x1000000;
    };

    for (int col = 0; col < 4; col++) {
        int x0 = col * ATLAS;
        int blades = 16 + (int)(rnd() * 6);
        for (int b = 0; b < blades; b++) {
            float baseX = x0 + ATLAS * (0.12f + 0.76f * rnd());
            float lean  = (rnd() * 2.0f - 1.0f) * ATLAS * 0.28f;
            float curve = (rnd() * 2.0f - 1.0f) * ATLAS * 0.18f;
            float hgt   = ATLAS * (0.55f + 0.43f * rnd());
            bool  dry   = rnd() < 0.18f;                       // straw-yellow blades
            // green ramp: dark base -> light tip (dry: dull yellow)
            float r0 = dry ? 0.42f : 0.10f, g0 = dry ? 0.38f : 0.26f, b0 = 0.07f;
            float r1 = dry ? 0.58f : 0.32f, g1 = dry ? 0.52f : 0.55f, b1 = dry ? 0.22f : 0.16f;
            int steps = (int)hgt * 2;
            for (int s = 0; s <= steps; s++) {
                float t = (float)s / steps;                    // 0 base -> 1 tip
                float x = baseX + lean * t + curve * t * t;
                float y = H - 1 - hgt * t;                     // atlas v=0 at top
                float wpx = (1.0f - t) * 2.6f + 0.4f;          // taper to the tip
                float rr = r0 + (r1 - r0) * t, gg = g0 + (g1 - g0) * t,
                      bb = b0 + (b1 - b0) * t;
                for (int dx = (int)-wpx; dx <= (int)wpx; dx++)
                    put((int)x + dx, (int)y, rr, gg, bb, 1.0f);
            }
            // wildflowers on one column: a bright speck at some tips
            if (col == 3 && rnd() < 0.30f) {
                float fx = baseX + lean + curve, fy = H - 1 - hgt;
                bool white = rnd() < 0.5f;
                for (int dy = -2; dy <= 2; dy++)
                    for (int dx = -2; dx <= 2; dx++)
                        if (dx * dx + dy * dy <= 4)
                            put((int)fx + dx, (int)fy + dy,
                                white ? 0.92f : 0.95f, white ? 0.92f : 0.80f,
                                white ? 0.85f : 0.25f, 1.0f);
            }
        }
    }
}
}  // namespace

bool Grass::init() {
    char base[512];
    const char* sdlBase = SDL_GetBasePath();
    snprintf(base, sizeof(base), "%s", sdlBase ? sdlBase : "");
    char vp[600], fp[600];
    snprintf(vp, sizeof(vp), "%sshaders/grass.vert", base);
    snprintf(fp, sizeof(fp), "%sshaders/grass.frag", base);
    if (!shader.load(vp, fp))
        if (!shader.load("shaders/grass.vert", "shaders/grass.frag")) return false;
    locAtlas4x4 = glGetUniformLocation(shader.program, "atlas4x4");

    // Clump mesh: three full-tile quads at 60° intervals, 1x1 m at scale 1. The
    // atlas tiles are whole bush clusters with transparent padding under the roots,
    // so the FULL tile is sampled (cropping slices the bush = hard vertical edges)
    // and instances are sunk below the terrain (see rebuild) to bury the padding —
    // same trick the tree cards use. Three planes read round from above.
    std::vector<float>    Vv;
    std::vector<unsigned> Iv;
    for (int k = 0; k < 3; k++) {
        float a = (float)k * 1.04719755f;   // 0, 60, 120 degrees
        float ax = cosf(a) * 0.5f, az = sinf(a) * 0.5f;
        unsigned b = (unsigned)(Vv.size() / 5);
        const float Q[] = {
            -ax, 0, -az,  0, 1,   ax, 0, az,  1, 1,   ax, 1, az,  1, 0,  -ax, 1, -az,  0, 0,
        };
        Vv.insert(Vv.end(), Q, Q + 20);
        unsigned qi[] = {b, b + 1, b + 2, b + 2, b + 3, b};
        Iv.insert(Iv.end(), qi, qi + 6);
    }
    indexCount = (GLsizei)Iv.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glGenBuffers(1, &instVBO);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, Vv.size() * sizeof(float), Vv.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, Iv.size() * sizeof(unsigned), Iv.data(), GL_STATIC_DRAW);
    const GLsizei vs = 5 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vs, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vs, (void*)(3 * sizeof(float)));
    // Per-instance: pos.xyz (2), yaw+scale (3), terrain normal (4), divisor 1.
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    const GLsizei is = 8 * sizeof(float);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, is, (void*)0);
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, is, (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, is, (void*)(5 * sizeof(float)));
    glVertexAttribDivisor(4, 1);
    glBindVertexArray(0);

    // Blade atlas: prefer the dedicated close-grass asset (4x4 tiles), cleaned on
    // load — the generated PNG has magenta edge spill and un-dilated transparent
    // texels that bleed pale into mip levels. Falls back to a procedural atlas.
    const char* candidates[] = {
        "textures/environment/close_grass_atlas_keyed_1024.png",
        "textures/environment/close_grass_atlas_1024.png",
    };
    int aw = 0, ah = 0;
    unsigned char* apx = nullptr;
    for (const char* rel : candidates) {
        char ap[600];
        snprintf(ap, sizeof(ap), "%s%s", base, rel);
        stbi_set_flip_vertically_on_load(0);
        int c = 0;
        apx = stbi_load(ap, &aw, &ah, &c, 4);
        if (!apx) apx = stbi_load(rel, &aw, &ah, &c, 4);
        if (apx) break;
    }
    if (apx) {
        // 1) Key out magenta spill (blue > green in a grass atlas = spill), then
        // flood every transparent texel's RGB with a neutral grass green — mipmap
        // generation averages RGB regardless of alpha, so leftover magenta under
        // alpha 0 would still tint the distance mips pink.
        size_t n = (size_t)aw * ah;
        for (size_t i = 0; i < n; i++) {
            unsigned char* p = apx + i * 4;
            if (p[2] > p[1] && p[0] > p[1]) p[3] = 0;
            // Binarize the AI atlas's feathered alpha: the soft halo zone otherwise
            // renders as pale ghost cards around every clump (alpha-tested feather =
            // translucent wash). Crisp 0/255 alpha = crisp blades, like a hand-cut
            // game atlas.
            p[3] = p[3] >= 110 ? 255 : 0;
            if (p[3] == 0) { p[0] = 74; p[1] = 96; p[2] = 44; }
        }
        // 2) Dilate blade colors into transparent texels (2 passes) so mip levels
        // average toward grass color instead of the void, killing pale halos.
        std::vector<unsigned char> src((size_t)aw * ah * 4);
        for (int pass = 0; pass < 2; pass++) {
            memcpy(src.data(), apx, src.size());
            for (int y = 0; y < ah; y++)
            for (int x = 0; x < aw; x++) {
                unsigned char* p = apx + ((size_t)y * aw + x) * 4;
                if (src[((size_t)y * aw + x) * 4 + 3] != 0) continue;
                int rs = 0, gs = 0, bs = 0, cnt = 0;
                for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= aw || ny < 0 || ny >= ah) continue;
                    const unsigned char* q = src.data() + ((size_t)ny * aw + nx) * 4;
                    if (q[3] == 0) continue;
                    rs += q[0]; gs += q[1]; bs += q[2]; cnt++;
                }
                if (cnt) { p[0] = rs / cnt; p[1] = gs / cnt; p[2] = bs / cnt; }
            }
        }
        glGenTextures(1, &atlasTex);
        glBindTexture(GL_TEXTURE_2D, atlasTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aw, ah, 0, GL_RGBA, GL_UNSIGNED_BYTE, apx);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(apx);
    }
    atlasIsAsset = atlasTex != 0;
    if (!atlasTex) {
        fprintf(stderr, "grass: close_grass atlas missing — using procedural blades\n");
        std::vector<unsigned char> px;
        paintAtlas(px);
        glGenTextures(1, &atlasTex);
        glBindTexture(GL_TEXTURE_2D, atlasTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS * 4, ATLAS, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, px.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    return true;
}

void Grass::clear() { count = 0; lastX = lastZ = 1e9f; }

void Grass::rebuild(const glm::vec3& eye) {
    inst.clear();
    const int   cells = (int)(GRASS_RADIUS / CELL);
    const int   ccx = (int)floorf(eye.x / CELL), ccz = (int)floorf(eye.z / CELL);
    const float R2 = GRASS_RADIUS * GRASS_RADIUS;
    const float e = 0.6f;   // finite-difference step for the terrain normal

    for (int gz = ccz - cells; gz <= ccz + cells; gz++)
    for (int gx = ccx - cells; gx <= ccx + cells; gx++) {
        float jx = mapRand(gx, gz, 71), jz = mapRand(gx, gz, 72);
        float x = (gx + jx) * CELL, z = (gz + jz) * CELL;
        if (fabsf(x) > gArenaHalf || fabsf(z) > gArenaHalf) continue;
        float dx = x - eye.x, dz = z - eye.z;
        float d2 = dx * dx + dz * dz;
        if (d2 > R2) continue;
        if (mapRand(gx, gz, 73) > keepAtDist(sqrtf(d2))) continue;

        float h = terrainHeight(x, z);
        if (gTerrainMode != TERRAIN_OFF && h < MIN_GROUND_Y) continue;  // beach/sea
        float hx1 = terrainHeight(x + e, z), hx0 = terrainHeight(x - e, z);
        float hz1 = terrainHeight(x, z + e), hz0 = terrainHeight(x, z - e);
        float sx = (hx1 - hx0) / (2 * e), sz = (hz1 - hz0) / (2 * e);
        if (sx * sx + sz * sz > MAX_SLOPE * MAX_SLOPE) continue;   // rocky steeps

        // Keep clumps out of buildings/cover (few boxes; cheap test).
        bool inside = false;
        for (int i = 0; i < gMapBoxCount && !inside; i++) {
            const Box& b = gMapBoxes[i];
            inside = fabsf(x - b.center.x) < b.half.x + 0.4f &&
                     fabsf(z - b.center.z) < b.half.z + 0.4f;
        }
        if (inside) continue;
        // Lobby: keep the firing lane and target-wall apron mowed (matches the
        // foliage lobby scatter's clearances).
        if (gMapId == MAP_LOBBY &&
            ((fabsf(z) < 13.0f && x > -14.0f) || (x > 22.0f && fabsf(z) < 17.0f)))
            continue;

        // Terrain normal from the height gradient (normalized).
        glm::vec3 n = glm::normalize(glm::vec3(-sx, 1.0f, -sz));
        float yaw   = mapRand(gx, gz, 74) * 6.2831853f;
        float scale = 0.95f + 0.75f * mapRand(gx, gz, 75);
        // Sink: bury the tile's transparent under-root padding plus the bottom of
        // the bush mass, so blades emerge from the soil and the card's bottom edge
        // (the star silhouette) never shows. Steeper ground buries deeper — on a
        // slope the card plane crosses the surface, so flat-ground sink isn't enough.
        float slopeMag = sqrtf(sx * sx + sz * sz);
        float sink = (0.16f + 0.55f * slopeMag) * scale;
        inst.insert(inst.end(), {x, h - sink, z, yaw, scale, n.x, n.y, n.z});
    }

    count = (int)(inst.size() / 8);
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    if ((GLint)inst.size() > instCap) {
        glBufferData(GL_ARRAY_BUFFER, inst.size() * sizeof(float), inst.data(),
                     GL_DYNAMIC_DRAW);
        instCap = (GLint)inst.size();
    } else if (count > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, inst.size() * sizeof(float), inst.data());
    }
    lastX = eye.x;
    lastZ = eye.z;
}

void Grass::draw(const Renderer& r, const glm::mat4& view, const glm::mat4& proj,
                 const glm::vec3& eye, float time) {
    float mx = eye.x - lastX, mz = eye.z - lastZ;
    if (mx * mx + mz * mz > REBUILD_STEP * REBUILD_STEP) rebuild(eye);
    if (count == 0) return;

    shader.use();
    shader.setMat4(shader.locView, view);
    shader.setMat4(shader.locProj, proj);
    shader.setVec3(shader.locEye, eye);
    shader.setFloat(shader.locTime, time);
    shader.setVec3(shader.locSunDir, r.sunDir);
    shader.setVec3(shader.locSkyZenith, r.skyZenith);
    shader.setVec3(shader.locSkyHorizon, r.skyHorizon);
    shader.setVec3(shader.locGroundAmb, r.groundAmbient);
    shader.setVec3(shader.locSunColor, r.sunColor);
    shader.setFloat(shader.locFogDist, r.fogDist);
    shader.setFloat(shader.locFogHeight, r.fogHeightAmt);
    shader.setFloat(shader.locCloud, r.cloudAmount);
    shader.setFloat(shader.locExposure, r.exposure);
    shader.setFloat(shader.locSaturation, r.saturation);
    shader.setMat4(shader.locLightSpace, r.lightSpace);
    shader.setInt(shader.locShadowMap, 1);
    shader.setInt(shader.locUseShadow, 1);
    shader.setInt(shader.locDiffuse, 0);
    shader.setInt(locAtlas4x4, atlasIsAsset ? 1 : 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, r.shadowTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTex);

    glDisable(GL_CULL_FACE);          // blades visible from both sides
    glBindVertexArray(vao);
    glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr, count);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}

void Grass::destroy() {
    if (atlasTex) glDeleteTextures(1, &atlasTex);
    if (instVBO) glDeleteBuffers(1, &instVBO);
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = instVBO = 0;
    atlasTex = 0;
    shader.destroy();
    inst.clear();
    count = 0;
}
