#pragma once
#include <glm/glm.hpp>
#include <cmath>

// Shared procedural heightfield for the outdoor maps. The same inline float math
// runs on the server (authoritative physics) and the client (mesh + prediction),
// so both agree on the ground height without any asset or network sync.
// terrainElevation() is the raw field for MAP_FIELD (designed heightmap or noise);
// lobbyElevation() shapes the training lobby (flat pad, hills toward the edge).
// terrainHeight() is the gameplay ground height, selected per map via gTerrainMode
// (set by setMap in map.h). Kept dependency-free (glm + cmath only) so map.h can
// include this without a circular include.

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

// --- training lobby ground (MAP_TRAINING) -----------------------------------
// Flat pad in the middle (covers the arena cover, firing range, and spawns), then
// blends into noise hills toward the edge. Always the procedural noise — never the
// field heightmap — so the lobby looks the same whether or not maps/field/height.png
// loaded.
inline constexpr float LOBBY_FLAT_R = 45.0f;   // fully flat inside this radius
inline constexpr float LOBBY_HILL_R = 100.0f;  // hills at full strength beyond this

inline float lobbyElevation(float x, float z) {
    float d = sqrtf(x * x + z * z);
    float t = terrSmooth(terrClamp01((d - LOBBY_FLAT_R) / (LOBBY_HILL_R - LOBBY_FLAT_R)));
    // Base lift keeps the ring reading as hills enclosing the pad, not random dips.
    return t * (5.0f + terrainProcedural(x, z));
}

// Ground shape for the active map (see setMap in map.h). TERRAIN_OFF keeps the
// flat-y=0 arena maps exactly as before.
enum TerrainMode { TERRAIN_OFF = 0, TERRAIN_FIELD, TERRAIN_LOBBY };
inline TerrainMode gTerrainMode = TERRAIN_OFF;

// Gameplay ground height used by physics/spawns/shadows.
inline float terrainHeight(float x, float z) {
    if (gTerrainMode == TERRAIN_FIELD) return terrainElevation(x, z);
    if (gTerrainMode == TERRAIN_LOBBY) return lobbyElevation(x, z);
    return 0.0f;
}
