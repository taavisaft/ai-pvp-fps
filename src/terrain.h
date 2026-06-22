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

// --- optional designed heightmap (filled by heightmap.cpp; same on client+server) ---
// When gHeightLoaded, terrainElevation samples this bilinearly instead of the noise
// below. gHeightData is W*H height values in metres, covering world XZ in
// [-gHeightHalf, +gHeightHalf]. Kept here (inline) so client + server sample identically.
inline bool         gHeightLoaded = false;
inline const float* gHeightData   = nullptr;
inline int          gHeightW = 0, gHeightH = 0;
inline float        gHeightHalf = 500.0f;

inline float terrClamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

inline float sampleHeightBilinear(float x, float z) {
    float u = terrClamp01((x + gHeightHalf) / (2.0f * gHeightHalf));
    float v = terrClamp01((z + gHeightHalf) / (2.0f * gHeightHalf));
    float fx = u * (gHeightW - 1), fz = v * (gHeightH - 1);
    int x0 = (int)fx, z0 = (int)fz;
    int x1 = x0 + 1 < gHeightW ? x0 + 1 : x0;
    int z1 = z0 + 1 < gHeightH ? z0 + 1 : z0;
    float tx = fx - x0, tz = fz - z0;
    const float* d = gHeightData;
    float h00 = d[z0 * gHeightW + x0], h10 = d[z0 * gHeightW + x1];
    float h01 = d[z1 * gHeightW + x0], h11 = d[z1 * gHeightW + x1];
    float a = h00 + (h10 - h00) * tx;
    float b = h01 + (h11 - h01) * tx;
    return a + (b - a) * tz;
}

// Procedural fallback: broad rolling hills + medium roll + fine bumps. Used only when
// no heightmap is loaded.
inline float terrainProcedural(float x, float z) {
    float h = 0.0f;
    h += (terrValueNoise(x * 0.012f, z * 0.012f) - 0.5f) * 16.0f; // broad hills ~±8 m
    h += (terrValueNoise(x * 0.05f,  z * 0.05f)  - 0.5f) * 4.0f;  // local roll  ~±2 m
    h += (terrValueNoise(x * 0.18f,  z * 0.18f)  - 0.5f) * 1.0f;  // fine bumps  ~±0.5 m
    return h;
}

// Raw elevation (metres) at world (x,z): designed heightmap if loaded, else noise.
inline float terrainElevation(float x, float z) {
    return gHeightLoaded ? sampleHeightBilinear(x, z) : terrainProcedural(x, z);
}

// Set true only while the field map is active (see setMap in map.h). When false,
// terrainHeight is flat 0 so the arena maps behave exactly as before.
inline bool gTerrainOn = false;

// Gameplay ground height used by physics/spawns/shadows.
inline float terrainHeight(float x, float z) {
    return gTerrainOn ? terrainElevation(x, z) : 0.0f;
}
