#pragma once

// First Paldiski combat slice: forestry shelter and staggered timber cover.
constexpr float FOREST_SITE_X = 325.0f;
constexpr float FOREST_SITE_Z = 75.0f;
void buildForestSite();
// Keep foliage out of structures, cover and approach paths, on both peers.
bool forestSiteClearance(float x, float z, float margin);
