#include "tree_collision.h"
#include "map.h"       // gMapId, mapRand, PALDISKI_HALF, LOBBY_HALF, MAP_LOBBY
#include "terrain.h"   // terrainHeight, pineForestBiome
#include <cmath>

std::vector<TreeInstance> gTrees;
std::vector<TreeCol>      gTreeCols;
SpatialGrid               gTreeColGrid;
float gTreeTrunkMaxRadius = 0.0f;

// Deterministic spruce scatter. Moved verbatim (math-identical) from the old
// Vegetation::buildTrees so the rendered forest and the collision cylinders agree.
static void scatterTrees(std::vector<TreeInstance>& out) {
    out.clear();
    if (gMapId == MAP_LOBBY) {
        // Loose spruce ring outside the pad, firing lane kept clear.
        for (int i = 0; i < 40; i++) {
            float a  = (i / 40.0f + (mapRand(i, 1, 61) - 0.5f) * 0.02f) * 6.2831853f;
            float rr = 34.0f + mapRand(i, 2, 62) * 22.0f;
            float x = cosf(a) * rr, z = sinf(a) * rr;
            if (x > 8.0f && fabsf(z) < 16.0f) continue;   // range lane to the wall
            TreeInstance t;
            t.x = x; t.z = z; t.y = terrainHeight(x, z) - 0.15f;
            t.scale = mapRand(i, 3, 63) < 0.4f ? 2.5f + mapRand(i, 4, 64) * 2.0f
                                               : 6.5f + mapRand(i, 4, 64) * 4.0f;
            t.yaw   = mapRand(i, 5, 65) * 6.2831853f;
            t.tint  = 0.82f + mapRand(i, 6, 66) * 0.36f;
            out.push_back(t);
        }
        return;
    }
    const float STEP = 5.0f;
    const int n = (int)(2.0f * PALDISKI_HALF / STEP);
    for (int iz = 0; iz < n; iz++)
        for (int ix = 0; ix < n; ix++) {
            float x = -PALDISKI_HALF + (ix + 0.5f) * STEP + (mapRand(ix, iz, 41) - 0.5f) * 4.2f;
            float z = -PALDISKI_HALF + (iz + 0.5f) * STEP + (mapRand(ix, iz, 42) - 0.5f) * 4.2f;
            if (forestSiteClearance(x, z, 1.0f)) continue;
            float b = pineForestBiome(x, z);
            float dens = b * b * 2.2f + b * 0.30f + 0.045f;
            if (mapRand(ix, iz, 43) > dens) continue;
            float h = terrainHeight(x, z);
            if (h < 1.6f || h > 95.0f) continue;
            float gx = (terrainHeight(x + 2.0f, z) - terrainHeight(x - 2.0f, z)) * 0.25f;
            float gz = (terrainHeight(x, z + 2.0f) - terrainHeight(x, z - 2.0f)) * 0.25f;
            if (gx * gx + gz * gz > 0.30f) continue;
            TreeInstance t;
            t.x = x; t.z = z; t.y = h - 0.15f;
            float r     = mapRand(ix, iz, 44);
            float young = 2.2f + r * 2.6f;               // 2.2-4.8 m sapling
            float grown = 6.5f + r * 5.0f;               // 6.5-11.5 m stand tree
            float pYoung = b < 0.35f ? 0.78f : 0.18f;    // meadow vs core mix
            t.scale = mapRand(ix, iz, 47) < pYoung ? young : grown;
            t.yaw   = mapRand(ix, iz, 45) * 6.2831853f;
            t.tint  = 0.82f + mapRand(ix, iz, 46) * 0.36f;
            out.push_back(t);
        }
}

void buildTreeColliders() {
    scatterTrees(gTrees);

    gTreeCols.clear();
    gTreeCols.reserve(gTrees.size());
    gTreeTrunkMaxRadius = 0.0f;
    for (const TreeInstance& t : gTrees) {
        gTreeCols.push_back({t.x, t.z, treeCollRadius(t.scale)});
        gTreeTrunkMaxRadius = fmaxf(gTreeTrunkMaxRadius, TREE_TRUNK_BASE * t.scale);
    }

    // XZ grid matching the render cull grid's resolution, so cell queries are cheap.
    float half  = (gMapId == MAP_LOBBY) ? LOBBY_HALF : PALDISKI_HALF;
    int   cells = (gMapId == MAP_LOBBY) ? 16 : 32;
    gTreeColGrid.init(half, cells, 0.0f, 1.0f);
    for (int i = 0; i < (int)gTreeCols.size(); i++)
        gTreeColGrid.insert(gTreeCols[i].x, gTreeCols[i].z, i);
}
