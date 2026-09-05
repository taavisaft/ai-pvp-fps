#include "forest_site.h"
#include "map.h"
#include <initializer_list>


namespace {
void timber(float x, float y, float z, float hx, float hy, float hz) {
    townPush({{FOREST_SITE_X + x, y, FOREST_SITE_Z + z}, {hx, hy, hz}}, SURF_WOOD);
}

// Squared sawn timber stacks: exact box silhouettes also serve as solid cover.
void stack(float x, float z, bool across, float height) {
    const float hx = across ? 2.8f : 0.7f, hz = across ? 0.7f : 2.8f;
    float base = terrainHeight(FOREST_SITE_X+x, FOREST_SITE_Z+z);
    float low = base, high = base;
    for (int a : {-1, 1}) for (int b : {-1, 1}) {
        float h = terrainHeight(FOREST_SITE_X+x+a*hx, FOREST_SITE_Z+z+b*hz);
        low = fminf(low, h); high = fmaxf(high, h);
    }
    // Buried timber crib follows the highest corner so stacks never float.
    float top = high + 0.18f;
    timber(x, (low+top)*0.5f, z, hx, (top-low)*0.5f+0.02f, hz);
    const int rows = (int)(height / 0.24f);
    for (int row = 0; row < rows; ++row) {
        float inset = (row % 2) * 0.10f;
        timber(x, top+0.12f+row*0.24f, z, hx-inset, 0.12f, hz-inset);
    }
}
}

void buildForestSite() {
    const float h = paldiskiBase(FOREST_SITE_X, FOREST_SITE_Z);
    gTerrainPads[gTerrainPadCount++] = {FOREST_SITE_X, FOREST_SITE_Z, 22.0f, h};
    // 8 x 6 m timber shelter. Ground-level floor permits walking through either
    // 1.8 m doorway; east/west windows offer peeks without sealing the side lanes.
    timber(0, h-0.10f, 0, 4.15f, 0.10f, 3.15f);
    for (float z : {-3.0f, 3.0f}) {
        timber(-2.45f, h+1.4f, z, 1.55f, 1.4f, 0.12f);
        timber( 2.45f, h+1.4f, z, 1.55f, 1.4f, 0.12f);
        timber(0, h+2.55f, z, 0.9f, 0.25f, 0.12f);
    }
    for (float x : {-4.0f, 4.0f}) {
        timber(x, h+0.55f, 0, 0.12f, 0.55f, 3);
        timber(x, h+2.55f, 0, 0.12f, 0.25f, 3);
        timber(x, h+1.7f, -2.05f, 0.12f, 0.6f, 0.95f);
        timber(x, h+1.7f,  2.05f, 0.12f, 0.6f, 0.95f);
    }
    // Shallow stepped shed roof: each visible panel is also collision geometry.
    for (int i = 0; i < 8; ++i) {
        float z = -3.5f + (i+0.5f)*0.875f;
        townPush({{FOREST_SITE_X, h+2.90f+i*0.055f, FOREST_SITE_Z+z},
                  {4.5f, 0.07f, 0.45f}}, SURF_METAL);
    }
    // Workbench and a crate leave the cross-shelter route clear.
    timber(2.8f, h+0.85f, 0.5f, 0.7f, 0.08f, 1.1f);
    timber(2.8f, h+0.40f, -0.3f, 0.5f, 0.40f, 0.15f);
    timber(2.8f, h+0.40f,  1.3f, 0.5f, 0.40f, 0.15f);
    timber(-2.6f, h+0.45f, -1.8f, 0.6f, 0.45f, 0.6f);
    // Staggered routes around the shelter: waist-high peeks and taller blockers.
    stack(-14,  11, true,  1.44f);
    stack( 12, -12, false, 1.92f);
    stack( 23,  15, true,  1.20f);
    stack(-25, -16, false, 1.68f);
    stack(  5,  31, true,  1.68f);
    stack( -7, -32, true,  1.44f);
    stack(-36,   5, false, 1.92f);
    stack( 36,  -5, false, 1.44f);
}

bool forestSiteClearance(float x, float z, float margin) {
    if (gTownBoxCount == 0) return false;
    if (fabsf(x-FOREST_SITE_X) > 55 || fabsf(z-FOREST_SITE_Z) > 55) return false;
    if (fabsf(x-(FOREST_SITE_X-20)) < 2+margin && fabsf(z-(FOREST_SITE_Z+18)) < 2+margin) return true;
    // Explicit north/south doorway approaches, plus the shelter footprint.
    if (fabsf(x-FOREST_SITE_X) < 1.6f+margin && fabsf(z-FOREST_SITE_Z) < 9+margin) return true;
    if (fabsf(x-FOREST_SITE_X) < 5+margin && fabsf(z-FOREST_SITE_Z) < 4+margin) return true;
    for (int i = 0; i < gTownBoxCount; ++i) {
        const Box& b = gTownBoxes[i];
        if (fabsf(x-b.center.x) < b.half.x+margin && fabsf(z-b.center.z) < b.half.z+margin) return true;
    }
    return false;
}
