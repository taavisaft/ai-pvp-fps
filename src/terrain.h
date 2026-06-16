#pragma once
#include <glm/glm.hpp>
#include <cmath>

// Shared procedural heightfield for the large outdoor map (MAP_FIELD). The same
// inline float math runs on the server (authoritative physics) and the client
// (mesh + prediction), so both agree on the ground height without any asset or
// network sync. terrainElevation() is the raw noise field; terrainHeight() is the
// gameplay ground height, which stays flat (0) on the small arena maps via the
// gTerrainOn flag (set by setMap in map.h). Kept dependency-free (glm + cmath
// only) so map.h can include this without a circular include.

// Integer hash → [0,1). Cheap, deterministic, platform-stable (pure int ops).
inline float terrHash(int x, int z) {
    unsigned n = (unsigned)(x * 374761393 + z * 668265263);
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= (n >> 16);
    return (float)(n & 0x7fffffffu) / (float)0x7fffffffu;
}

inline float terrSmooth(float t) { return t * t * (3.0f - 2.0f * t); }

// Value noise: bilinear blend of cell-corner hashes, smoothstep-interpolated.
inline float terrValueNoise(float x, float z) {
    float xf = floorf(x), zf = floorf(z);
    int   xi = (int)xf,   zi = (int)zf;
    float tx = terrSmooth(x - xf), tz = terrSmooth(z - zf);
    float a = terrHash(xi,     zi);
    float b = terrHash(xi + 1, zi);
    float c = terrHash(xi,     zi + 1);
    float d = terrHash(xi + 1, zi + 1);
    float ab = a + (b - a) * tx;
    float cd = c + (d - c) * tx;
    return ab + (cd - ab) * tz;   // [0,1)
}

// Raw elevation (metres) at world (x,z), independent of the active map. A few
// octaves: broad rolling hills + medium roll + fine bumps. Gentle on purpose so
// movement and bullet marching stay well-behaved.
inline float terrainElevation(float x, float z) {
    float h = 0.0f;
    h += (terrValueNoise(x * 0.012f, z * 0.012f) - 0.5f) * 16.0f; // broad hills ~±8 m
    h += (terrValueNoise(x * 0.05f,  z * 0.05f)  - 0.5f) * 4.0f;  // local roll  ~±2 m
    h += (terrValueNoise(x * 0.18f,  z * 0.18f)  - 0.5f) * 1.0f;  // fine bumps  ~±0.5 m
    return h;
}

// Set true only while the field map is active (see setMap in map.h). When false,
// terrainHeight is flat 0 so the arena maps behave exactly as before.
inline bool gTerrainOn = false;

// Gameplay ground height used by physics/spawns/shadows.
inline float terrainHeight(float x, float z) {
    return gTerrainOn ? terrainElevation(x, z) : 0.0f;
}
