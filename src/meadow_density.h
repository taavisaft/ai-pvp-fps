#pragma once
#include <cmath>

// Mirrored in veg.vert and veg_depth.vert. Keep a sparse population across
// the whole lobby; only individual clumps transition, never the entire field.
inline float meadowDensity(float distance) {
    float d=fmaxf(0,distance-6.0f)/8.0f;
    return fmaxf(.004f,1.18f/(1.0f+d*d));
}
inline float meadowRank(float phase) {
    float value=phase*13.37f;
    return value-floorf(value);
}

// Streaming residency ends beyond this gradual per-clump thinning band.
inline float worldMeadowDensity(float distance) {
    float t=fmaxf(0,fminf(1,(distance-40.0f)/10.0f));
    return meadowDensity(distance)*(1-t*t*(3-2*t));
}

inline int meadowTileSlot(int tx, int tz) {
    return ((tz%24+24)%24)*24+(tx%24+24)%24;
}
