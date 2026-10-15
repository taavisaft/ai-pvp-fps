#pragma once
#include "map.h"

// Visual habitat mask mirrored by woodlandCover() in forest_floor.glsl.
// Ground material and streamed grass use the same boundary; no new scatter.
inline float forestFloorCover(float x, float z, float h) {
    float forest = gMapId==MAP_LOBBY ? trainingForest(x,z) : pineForestBiome(x,z);
    float cover=lobbySmooth(.10f,.70f,forest);
    if(gMapId==MAP_PALDISKI) {
        // Match the tree habitat: river shallows and high rock are not woodland.
        cover*=lobbySmooth(1.6f,2.6f,h)*(1-lobbySmooth(90.0f,95.0f,h));
    }
    return cover;
}
