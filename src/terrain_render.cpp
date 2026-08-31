#include "terrain_render.h"
#include "map.h"   // PALDISKI_HALF + terrainHeight

constexpr float TerrainChunks::LOD_STEP[];
constexpr float TerrainChunks::LOD_DIST[];

// Vista height: the real terrain, dipped near the play boundary so the slab's
// coarse edge tucks underneath the fine chunks' skirts instead of poking through.
static float vistaElev(float x, float z) {
    float h = terrainHeight(x, z);
    float bx = fabsf(fabsf(x) - PALDISKI_HALF);
    float bz = fabsf(fabsf(z) - PALDISKI_HALF);
    float nearEdge = 1.0f - terrClamp01(fminf(bx, bz) / 96.0f);
    return h - 4.0f * nearEdge;
}

void TerrainChunks::draw(const Frustum& fr, const glm::vec3& eye, bool withVista) {
    const float half = PALDISKI_HALF;
    int builds = 0;   // expensive-build budget this call (hitch control)

    for (int cz = 0; cz < CHUNKS; cz++)
    for (int cx = 0; cx < CHUNKS; cx++) {
        Chunk& c = chunks[cx][cz];
        float x0 = -half + cx * CHUNK_SIZE;
        float z0 = -half + cz * CHUNK_SIZE;
        glm::vec3 center(x0 + CHUNK_SIZE * 0.5f, 0.0f, z0 + CHUNK_SIZE * 0.5f);

        if (c.boundsKnown) {
            glm::vec3 bc(center.x, (c.minY + c.maxY) * 0.5f, center.z);
            glm::vec3 bh(CHUNK_SIZE * 0.5f, (c.maxY - c.minY) * 0.5f + 4.0f,
                         CHUNK_SIZE * 0.5f);
            if (!fr.aabbVisible(bc, bh)) continue;
        }

        float dx = center.x - eye.x, dz = center.z - eye.z;
        float dist = sqrtf(dx * dx + dz * dz);
        int want = LODS - 1;
        for (int l = 0; l < LODS; l++)
            if (dist < LOD_DIST[l]) { want = l; break; }

        // The coarse tier is cheap enough to build inline whenever it's missing —
        // it doubles as the instant fallback while finer tiers wait their turn.
        if (!c.built[LODS - 1]) {
            float mn, mx;
            createTerrainPatch(c.lod[LODS - 1], x0, z0, CHUNK_SIZE, CHUNK_SIZE, LOD_STEP[LODS - 1],
                               1.5f + LOD_STEP[LODS - 1] * 0.5f, terrainHeight, &mn, &mx);
            c.built[LODS - 1] = true;
            if (!c.boundsKnown) { c.minY = mn; c.maxY = mx; c.boundsKnown = true; }
        }
        // Fine tiers: at most N expensive builds per frame; draw the best built
        // tier meanwhile (a one-frame-late LOD upgrade is invisible, a hitch isn't).
        if (want < LODS - 1 && !c.built[want] && builds < maxBuildsPerFrame) {
            createTerrainPatch(c.lod[want], x0, z0, CHUNK_SIZE, CHUNK_SIZE, LOD_STEP[want],
                               1.5f + LOD_STEP[want] * 0.5f, terrainHeight);
            c.built[want] = true;
            builds++;
        }
        int use = want;
        while (use < LODS && !c.built[use]) use++;
        c.lod[use].draw();
    }

    if (withVista) {
        if (!vistaBuilt) {
            // Four slabs surrounding the play area — the vista NEVER overlaps the
            // fine chunks, so there is nothing to z-fight at long range.
            const float V = VISTA_HALF, P = PALDISKI_HALF, S = VISTA_STEP;
            createTerrainPatch(vistaSlab[0], -V, -V, V - P, 2 * V, S, 0, vistaElev); // west
            createTerrainPatch(vistaSlab[1],  P, -V, V - P, 2 * V, S, 0, vistaElev); // east
            createTerrainPatch(vistaSlab[2], -P, -V, 2 * P, V - P, S, 0, vistaElev); // south
            createTerrainPatch(vistaSlab[3], -P,  P, 2 * P, V - P, S, 0, vistaElev); // north
            vistaBuilt = true;
        }
        for (Mesh& m : vistaSlab) m.draw();
    }
}

void TerrainChunks::destroy() {
    for (int cz = 0; cz < CHUNKS; cz++)
    for (int cx = 0; cx < CHUNKS; cx++)
        for (int l = 0; l < LODS; l++) {
            chunks[cx][cz].lod[l].destroy();
            chunks[cx][cz].built[l] = false;
            chunks[cx][cz].boundsKnown = false;
        }
    for (Mesh& m : vistaSlab) m.destroy();
    vistaBuilt = false;
}
