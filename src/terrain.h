#pragma once
#include <glm/glm.hpp>
#include <cmath>

// Shared procedural heightfield for PALDISKI (the one map, for now). The same inline
// float math runs on the server (authoritative physics) and the client (mesh +
// prediction), so both agree on the ground height without any asset or network sync.
// Kept dependency-free (glm + cmath only) so map.h can include this without a cycle.
//
// Paldiski layout (Estonian coastal town): the Baltic on the WEST (x negative side)
// behind a wavy shoreline, a beach band rising inland, a steeper Pakri-style bank on
// the north stretch of the coast, rolling hills growing toward the east edge, and a
// handful of flattened "pads" the shore town's buildings sit on (filled by
// generatePaldiski in map.h). Sea level is y = 0.

// Integer hash → [0,1). Cheap, deterministic, platform-stable (pure int ops).
inline float terrHash(int x, int z) {
    unsigned n = (unsigned)(x * 374761393 + z * 668265263);
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= (n >> 16);
    return (float)(n & 0x7fffffffu) / (float)0x7fffffffu;
}

inline float terrSmooth(float t) { return t * t * (3.0f - 2.0f * t); }
inline float terrClamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

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

// Broad inland zones occupied by mature, open-canopy pine forest. This analytical
// mask is cheap enough for both terrain shading and nearby grass rejection.
// 0 = meadow/mixed woodland, 1 = needle-and-moss pine floor.
inline float pineForestBiome(float x, float z) {
    auto ellipse = [](float px, float pz, float cx, float cz, float rx, float rz) {
        float dx = (px - cx) / rx, dz = (pz - cz) / rz;
        float d = sqrtf(dx * dx + dz * dz);
        return 1.0f - terrSmooth(terrClamp01((d - 0.66f) / 0.34f));
    };
    float south = ellipse(x, z, 150.0f, -105.0f, 108.0f, 132.0f);
    float north = ellipse(x, z, 170.0f,  135.0f,  82.0f,  88.0f);
    return fmaxf(south, north);
}

// --- flattened building pads (filled deterministically by generatePaldiski) -----
// Terrain blends toward each pad's height inside its radius so buildings sit on
// level ground. Few pads, so the per-sample loop is cheap enough for physics.
constexpr int MAX_TERRAIN_PADS = 16;
struct TerrainPad { float x, z, r, h; };
inline TerrainPad gTerrainPads[MAX_TERRAIN_PADS];
inline int        gTerrainPadCount = 0;

// World X of the waterline's underwater base at a given Z (the beach crosses y=0 a
// few meters east of this). Shared so the town generator can hug the coast.
inline float paldiskiShoreX(float z) {
    return -90.0f + (terrValueNoise(z * 0.015f + 31.0f, 13.7f) - 0.5f) * 70.0f;
}

// Raw Paldiski elevation, before pad flattening.
inline float paldiskiBase(float x, float z) {
    float d = x - paldiskiShoreX(z);            // <0 seaward, >0 landward (m)
    float tSea  = terrClamp01(-d / 70.0f);      // 0 at shore -> 1 out at sea
    float tLand = terrClamp01(d / 60.0f);       // beach/bank rise band
    float h = -1.0f - tSea * 7.0f + terrSmooth(tLand) * 7.0f;   // -8 m ... +6 m

    // Pakri flavor: the northern stretch of coast rises as a steeper, higher bank.
    float cliff = terrSmooth(terrClamp01((-z - 60.0f) / 60.0f));
    h += cliff * terrSmooth(terrClamp01(d / 25.0f)) * 6.0f;

    // Rolling hills grow inland (east); clamped upward so no inland lakes form.
    float inland = terrSmooth(terrClamp01((d - 80.0f) / 140.0f));
    float hills  = (terrValueNoise(x * 0.012f, z * 0.012f) - 0.5f) * 22.0f
                 + (terrValueNoise(x * 0.05f,  z * 0.05f)  - 0.5f) * 4.0f;
    h += inland * fmaxf(hills, 0.0f);

    // Fine bumps on land only (the sea floor stays smooth).
    if (d > 0.0f)
        h += (terrValueNoise(x * 0.18f, z * 0.18f) - 0.5f) * 0.6f * terrSmooth(tLand);
    return h;
}

inline float paldiskiElevation(float x, float z) {
    float h = paldiskiBase(x, z);
    for (int i = 0; i < gTerrainPadCount; i++) {
        const TerrainPad& p = gTerrainPads[i];
        float dx = x - p.x, dz = z - p.z;
        float dist = sqrtf(dx * dx + dz * dz);
        if (dist >= p.r) continue;
        // Plateau: dead flat out to (r - falloff) — the building footprint must sit
        // fully inside that — then a smooth 12 m skirt down to the natural ground.
        constexpr float FALLOFF = 12.0f;
        float w = terrSmooth(terrClamp01((p.r - dist) / FALLOFF));
        h = h + (p.h - h) * w;
    }
    return h;
}

// Ground shape for the active map (see setMap in map.h). TERRAIN_OFF = flat y=0,
// kept so future arena-style maps can opt out of the heightfield.
enum TerrainMode { TERRAIN_OFF = 0, TERRAIN_PALDISKI, TERRAIN_LOBBY };
inline TerrainMode gTerrainMode = TERRAIN_OFF;

// Gameplay ground height used by physics/spawns/shadows/mesh.
inline float terrainHeight(float x, float z) {
    if (gTerrainMode == TERRAIN_PALDISKI) return paldiskiElevation(x, z);
    if (gTerrainMode == TERRAIN_LOBBY) {
        float lane = terrSmooth(terrClamp01((fabsf(z) - 14.0f) / 12.0f));
        float edge = terrSmooth(terrClamp01((sqrtf(x*x + z*z) - 18.0f) / 35.0f));
        float broad = (terrValueNoise(x * 0.035f + 8.0f, z * 0.035f - 3.0f) - 0.42f) * 5.0f;
        float detail = (terrValueNoise(x * 0.11f, z * 0.11f) - 0.5f) * 0.7f;
        return fmaxf(0.0f, (broad + detail) * fmaxf(lane, edge));
    }
    return 0.0f;
}
