#include "grass.h"
#include "renderer.h"   // full Renderer type (lighting palette + shadow map)
#include "map.h"         // terrainHeight, gTownBoxes, mapHash/mapRand
#include <SDL.h>
#include <cstdio>
#include <cmath>

// --- tuning ------------------------------------------------------------------
namespace {
constexpr float GRASS_RADIUS   = 52.0f;  // dense near field; flat terrain carries the horizon
constexpr float CELL           = 0.64f;  // small overlapping tufts, not metre-wide bushes
constexpr float REBUILD_STEP   = 2.5f;   // camera travel that triggers a rebuild (m)
constexpr float MIN_GROUND_Y   = 1.35f;  // no grass on beach/sea (matches sand splat)
constexpr float MAX_SLOPE      = 0.55f;  // no grass on rocky steeps (matches trees)
constexpr float BASE_KEEP      = 0.94f;  // first tuft probability on ideal ground
constexpr int   ATLAS          = 256;    // atlas cell size (px), 4 columns

// Density only thins gently. A steep falloff exposes isolated cards and is the main
// reason procedural grass reads as scattered weeds rather than a continuous meadow.
inline float keepAtDist(float d) {
    if (d < 24.0f) return BASE_KEEP;
    return BASE_KEEP * (1.0f - 0.32f * (d - 24.0f) / (GRASS_RADIUS - 24.0f));
}

// --- procedural blade atlas ---------------------------------------------------
// Four painted blade clusters. Unlike the old AI atlas, these contain separate thin
// blades rather than a solid bush-shaped base, so crossed cards do not expose stars.
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
        int blades = 24 + (int)(rnd() * 9);
        for (int b = 0; b < blades; b++) {
            float baseX = x0 + ATLAS * (0.12f + 0.76f * rnd());
            float lean  = (rnd() * 2.0f - 1.0f) * ATLAS * 0.28f;
            float curve = (rnd() * 2.0f - 1.0f) * ATLAS * 0.18f;
            float hgt   = ATLAS * (0.55f + 0.43f * rnd());
            bool  dry   = rnd() < 0.18f;                       // straw-yellow blades
            // green ramp: dark base -> light tip (dry: dull yellow)
            float r0 = dry ? 0.43f : 0.17f, g0 = dry ? 0.38f : 0.27f,
                  b0 = dry ? 0.12f : 0.08f;
            float r1 = dry ? 0.64f : 0.36f, g1 = dry ? 0.56f : 0.48f,
                  b1 = dry ? 0.27f : 0.17f;
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
            // Sparse pale seed heads on one variant.
            if (col == 3 && rnd() < 0.22f) {
                float fx = baseX + lean + curve, fy = H - 1 - hgt;
                for (int dy = -3; dy <= 2; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        put((int)fx + dx, (int)fy + dy, 0.68f, 0.61f, 0.32f, 1.0f);
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

    // Clump mesh: four full-tile quads at 45° intervals, 1x1 m at scale 1. The
    // atlas tiles are whole bush clusters with transparent padding under the roots,
    // so the FULL tile is sampled (cropping slices the bush = hard vertical edges)
    // and instances are sunk below the terrain (see rebuild) to bury the padding —
    // same trick the tree cards use. Four planes remain full from oblique FPS views
    // without needing camera-facing billboards.
    std::vector<float>    Vv;
    std::vector<unsigned> Iv;
    for (int k = 0; k < 4; k++) {
        float a = (float)k * 0.78539816f;   // 0, 45, 90, 135 degrees
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
    atlasIsAsset = false;
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
    for (int gx = ccx - cells; gx <= ccx + cells; gx++)
    for (int layer = 0; layer < 2; layer++) {
        // A nearly-complete base layer plus a sparser offset layer. Each card is
        // small, but their silhouettes overlap into the continuous DayZ-style mat.
        float layerKeep = layer == 0 ? 1.0f : 0.86f;
        if (mapRand(gx, gz, 173 + layer) > keepAtDist(0.0f) * layerKeep) continue;
        float jx = mapRand(gx, gz, 71 + layer * 11);
        float jz = mapRand(gx, gz, 72 + layer * 11);
        float x = (gx + jx) * CELL, z = (gz + jz) * CELL;
        if (fabsf(x) > gArenaHalf || fabsf(z) > gArenaHalf) continue;
        float dx = x - eye.x, dz = z - eye.z;
        float d2 = dx * dx + dz * dz;
        if (d2 > R2) continue;
        if (mapRand(gx, gz, 73 + layer * 11) >
            keepAtDist(sqrtf(d2)) / BASE_KEEP) continue;

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
        // Lobby: keep only a narrow worn firing path and the immediate target apron
        // mowed. The old 26 m-wide strip made the whole spawn view look barren.
        if (gMapId == MAP_LOBBY &&
            ((fabsf(z) < 1.8f && x > -14.0f) || (x > 30.0f && fabsf(z) < 5.0f)))
            continue;

        // Terrain normal from the height gradient (normalized).
        glm::vec3 n = glm::normalize(glm::vec3(-sx, 1.0f, -sz));
        float yaw = mapRand(gx, gz, 74 + layer * 11) * 6.2831853f;
        float r   = mapRand(gx, gz, 75 + layer * 11);
        // Most of the field is knee-low. Sparse taller seed-head clumps break the
        // skyline without turning every square metre into a one-metre bush.
        float scale = (r < 0.88f) ? (0.44f + 0.30f * (r / 0.88f))
                                  : (0.72f + 0.38f * ((r - 0.88f) / 0.12f));
        // Sink: bury the tile's transparent under-root padding plus the bottom of
        // the bush mass, so blades emerge from the soil and the card's bottom edge
        // (the star silhouette) never shows. Steeper ground buries deeper — on a
        // slope the card plane crosses the surface, so flat-ground sink isn't enough.
        float slopeMag = sqrtf(sx * sx + sz * sz);
        float sink = (0.035f + 0.28f * slopeMag) * scale;
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
