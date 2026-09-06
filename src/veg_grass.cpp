#include "vegetation.h"
#include "renderer.h"
#include "map.h"
#include <cstdio>

static float sstep(float a, float b, float v) { return lobbySmooth(a,b,v); }

// Fill one 16 m grass tile: jittered candidates masked by the same dirt-field /
// forest-floor / sand / slope rules the ground shader colors by, so blades stand
// exactly where the ground reads grassy. Build-time only work.
void Vegetation::rebuildTile(GrassTile& t, int tx, int tz) {
    bufTile.clear();
    const float x0 = tx * GRASS_TILE, z0 = tz * GRASS_TILE;
    const bool lobby = gMapId == MAP_LOBBY;
    const int candidates = (int)(GRASS_TILE * GRASS_TILE * (lobby ? 12.0f : GRASS_PER_M2));
    t.minY = 1e9f; t.maxY = -1e9f;
    for (int i = 0; i < candidates; i++) {
        int cx = tx * 4096 + i;   // unique hash coords per candidate
        float rx = x0 + mapRand(cx, tz, 71) * GRASS_TILE;
        float rz = z0 + mapRand(cx, tz, 72) * GRASS_TILE;
        if (fabsf(rx) > gArenaHalf || fabsf(rz) > gArenaHalf) continue;
        float h = terrainHeight(rx, rz);
        if (!lobby && (h < 1.25f || h > 95.0f)) continue;            // sand/water & alpine rock
        float sx = terrainHeight(rx + 1.2f, rz) - h;
        float sz = terrainHeight(rx, rz + 1.2f) - h;
        if (sx * sx + sz * sz > 0.55f) continue;         // steep = rock face
        float region = vegFbm(rx * 0.004f, rz * 0.004f); // dirt fields: sparse
        float keep = (1.0f - sstep(0.42f, 0.60f, region) * 0.9f)
                   * (1.0f - 0.65f * pineForestBiome(rx, rz));
        if (lobby) {
            keep = (1-(meadowEnabled ? lobbyMeadow(rx,rz) : 0)) * (1-lobbyWear(rx,rz)) * (0.38f + 0.62f*vegFbm(rx*.23f,rz*.23f));
            for (int j = 0; j < gMapBoxCount; ++j) {
                const Box& b = gMapBoxes[j];
                if (fabsf(rx-b.center.x)<b.half.x+.3f && fabsf(rz-b.center.z)<b.half.z+.3f) {
                    keep = 0; break;
                }
            }
        }
        if (mapRand(cx, tz, 73) > keep) continue;
        float dry = sstep(0.72f, 0.88f, vegFbm(rx * 0.4f + 10.0f, rz * 0.4f + 10.0f));
        if (lobby) dry = vegFbm(rx*.17f+3,rz*.17f)*1.1f;
        float scale = lobby ? .35f + mapRand(cx,tz,74)*.85f : .55f + mapRand(cx,tz,74)*.55f;
        bufTile.insert(bufTile.end(),
            {rx, h - 0.02f, rz, scale,
             mapRand(cx, tz, 75) * 6.2831853f, mapRand(cx, tz, 76),
             0.80f + mapRand(cx, tz, 77) * 0.45f, dry * 0.55f});
        if (h < t.minY) t.minY = h;
        if (h > t.maxY) t.maxY = h;
    }
    if (t.minY > t.maxY) { t.minY = 0.0f; t.maxY = 0.0f; }
    if (!t.vbo) glGenBuffers(1, &t.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, t.vbo);
    glBufferData(GL_ARRAY_BUFFER, bufTile.size() * sizeof(float), bufTile.data(),
                 GL_STATIC_DRAW);
    if (!t.vao) t.vao = vegMakeVAO(bladeVbo, bladeEbo, t.vbo);
    t.count = (int)(bufTile.size() / 8);
    t.tx = tx; t.tz = tz;
}

// All lobby tiles are uploaded once at map initialization; walking never builds
// tiles or grows staging buffers. Paldiski grass remains disabled by quality.
void Vegetation::prepareLobbyGrass() {
    meadowEnabled = getenv("FPS_NOMEADOW") == nullptr;
    bufTile.reserve((size_t)(GRASS_TILE*GRASS_TILE*12)*8);
    int count = 0;
    for (int z=-4; z<4; ++z) for (int x=-4; x<4; ++x) {
        GrassTile& t = tiles[(x+GRASS_SLOTS)%GRASS_SLOTS][(z+GRASS_SLOTS)%GRASS_SLOTS];
        rebuildTile(t,x,z);
        count += t.count;
    }
    if (meadowEnabled) prepareMeadow();
    printf("[lobby grass] %d tufts, 64 static tiles, range 38 m\n", count);
}

void Vegetation::drawGrass(const Frustum& fr, const glm::vec3& eye) {
    // Grass: camera-centered tile ring; stale slots rebuilt within a budget (a
    // fresh slot's blades are still height-zero at the range edge, so a one-frame
    // delay is invisible).
    const bool lobby = gMapId == MAP_LOBBY;
    const float range = lobby ? 38.0f : GRASS_RANGE;
    if (lobby || grassEnabled_) {
    vegSh.setFloat(locWind, 0.045f);
    vegSh.setFloat(locRange, range);
    glUniform2f(locFadeIn, 0.0f, 0.0f);
    glUniform2f(locFadeOut, 0.0f, 0.0f);
    const int S = GRASS_SLOTS;
    int ctx = (int)floorf(eye.x / GRASS_TILE);
    int ctz = (int)floorf(eye.z / GRASS_TILE);
    // Rebuild budget: a tile build costs ~1k terrain samples, so keep the steady-
    // state trickle small; only a fresh map (everything stale) gets a big burst.
    int stale = 0;
    for (auto& row : tiles) for (auto& t : row) if (t.tx == INT_MIN) stale++;
    int budget = stale > 120 ? 60 : 3;
    for (int dz = -GRASS_RING; dz <= GRASS_RING; dz++)
        for (int dx = -GRASS_RING; dx <= GRASS_RING; dx++) {
            int tx = ctx + dx, tz = ctz + dz;
            if (lobby && (tx < -4 || tx > 3 || tz < -4 || tz > 3)) continue;
            float x0 = tx * GRASS_TILE, z0 = tz * GRASS_TILE;
            float nx = fmaxf(fabsf(eye.x - (x0 + GRASS_TILE * 0.5f)) - GRASS_TILE * 0.5f, 0.0f);
            float nz = fmaxf(fabsf(eye.z - (z0 + GRASS_TILE * 0.5f)) - GRASS_TILE * 0.5f, 0.0f);
            if (nx * nx + nz * nz > range * range) continue;
            GrassTile& t = tiles[(tx % S + S) % S][(tz % S + S) % S];
            if (t.tx != tx || t.tz != tz) {
                if (lobby || budget <= 0) continue;
                budget--;
                rebuildTile(t, tx, tz);
            }
            if (!t.count) continue;
            glm::vec3 c(x0 + GRASS_TILE * 0.5f, (t.minY + t.maxY) * 0.5f,
                        z0 + GRASS_TILE * 0.5f);
            glm::vec3 half(GRASS_TILE * 0.5f, (t.maxY - t.minY) * 0.5f + 0.9f,
                           GRASS_TILE * 0.5f);
            if (!fr.aabbVisible(c, half)) continue;
            glBindVertexArray(t.vao);
            glDrawElementsInstanced(GL_TRIANGLES, bladeIdx, GL_UNSIGNED_INT, nullptr,
                                    t.count);
        }
    }

}
