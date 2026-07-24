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

// Broad taiga forest mask: large noise patches of dense conifer stand vs open
// bog-meadow (the reference's alternating plain). 0 = open, 1 = dense forest floor.
inline float pineForestBiome(float x, float z) {
    float f = terrValueNoise(x * 0.0016f + 91.0f, z * 0.0016f + 47.0f) * 0.7f
            + terrValueNoise(x * 0.006f, z * 0.006f) * 0.3f;
    return terrSmooth(terrClamp01((f - 0.46f) / 0.16f));
}

// --- flattened building pads (filled deterministically by generatePaldiski) -----
// Terrain blends toward each pad's height inside its radius so buildings sit on
// level ground. Few pads, so the per-sample loop is cheap enough for physics.
constexpr int MAX_TERRAIN_PADS = 16;
struct TerrainPad { float x, z, r, h; };
inline TerrainPad gTerrainPads[MAX_TERRAIN_PADS];
inline int        gTerrainPadCount = 0;

// World X of the waterline's underwater base at a given Z (the shore crosses y=0 a
// little east of this). Big lazy curves — a lake/sea edge, not a ragged beach.
inline float paldiskiShoreX(float z) {
    return -680.0f + (terrValueNoise(z * 0.0035f + 31.0f, 13.7f) - 0.5f) * 260.0f
                   + (terrValueNoise(z * 0.012f + 7.0f,  91.2f) - 0.5f) * 60.0f;
}

// Smooth terracing: subtracts a sine ripple from the height so slopes break into
// banded steps (small cliff bands like a cut river gorge), flats stay untouched.
inline float terrTerrace(float v, float period, float k) {
    return v - sinf(v * 6.2831853f / period) * (period / 6.2831853f) * k;
}

// Distance from (x,z) to the polyline p[0..n-1] — river spline carving.
inline float terrPolyDist(float x, float z, const float (*p)[2], int n) {
    float best = 1e9f;
    for (int i = 0; i + 1 < n; i++) {
        float ax = p[i][0], az = p[i][1], bx = p[i + 1][0], bz = p[i + 1][1];
        float abx = bx - ax, abz = bz - az;
        float t = ((x - ax) * abx + (z - az) * abz) / (abx * abx + abz * abz);
        t = terrClamp01(t);
        float dx = x - (ax + abx * t), dz = z - (az + abz * t);
        float d = sqrtf(dx * dx + dz * dz);
        if (d < best) best = d;
    }
    return best;
}

// Raw taiga elevation, before pad flattening. Layout (2x2 km play area, ±1024):
// water WEST behind a lazy shoreline, rolling taiga hills with terraced (cliffy)
// banks, two rivers cut west toward the water — gorges through the hills, flooded
// inlets near the coast — and a mountain range ramping up beyond the EAST edge
// (playable foothills inside, snowy vista peaks outside the clamp).
inline float paldiskiBase(float x, float z) {
    float d = x - paldiskiShoreX(z);            // <0 seaward, >0 landward (m)
    float tSea  = terrClamp01(-d / 240.0f);     // long shallow shelf out to sea
    float tLand = terrClamp01(d / 90.0f);       // shore bank rise
    float h = -1.5f - tSea * 20.0f + terrSmooth(tLand) * 4.0f;   // deep enough that
    // far water and seabed never sit inside the depth buffer's distant error band

    // Rolling taiga hills inland: broad 300-500 m swells + medium roll + fine bumps.
    // Kept as one explicit multiplier so we can test tall terrain in isolation.
    constexpr float RELIEF = 2.25f;
    float inland = terrSmooth(terrClamp01((d - 60.0f) / 160.0f));
    float hills  = (terrValueNoise(x * 0.004f,  z * 0.004f)  - 0.5f) * 46.0f * RELIEF
                 + (terrValueNoise(x * 0.016f,  z * 0.016f)  - 0.5f) * 10.0f;
    // Terrace the swells: slopes break into small cliff bands, flats stay smooth.
    hills = terrTerrace(hills, 13.0f, 0.55f);
    h += inland * fmaxf(hills, -4.0f * RELIEF);
    h += inland * (terrValueNoise(x * 0.07f, z * 0.07f) - 0.5f) * 1.8f;

    // Mountain ramp: starts near the east play edge, peaks far outside the clamp.
    // Ridged noise gives sharp crests; the vista mesh shows the range, the last
    // foothill slopes are still walkable inside +/-1024.
    float mx = terrClamp01((x - 650.0f) / 2000.0f);
    if (mx > 0.0f) {
        float r1 = 1.0f - fabsf(2.0f * terrValueNoise(x * 0.0016f, z * 0.0016f) - 1.0f);
        float r2 = 1.0f - fabsf(2.0f * terrValueNoise(x * 0.006f + 40.0f, z * 0.006f) - 1.0f);
        h += powf(terrSmooth(mx), 1.6f) * (420.0f + r1 * 520.0f + r2 * 90.0f);
    }

    // Rivers: two channels draining west. Near the carve line the ground is forced
    // toward -2 m (below water -> flooded river); through high ground that force
    // cuts a deep gorge with the terraced walls doing the cliff work.
    static const float R1[][2] = {{980, -350}, {520, -390}, {150, -290}, {-260, -390}, {-780, -430}};
    static const float R2[][2] = {{980,  320}, {470,  250}, {90,   390}, {-380,  290}, {-780,  250}};
    // Land floor: away from the shore the ground never dips below ~+1.7 m on its
    // own — no accidental inland seas. Only the river carve below may cut deeper.
    float landFloor = -1.5f + terrSmooth(terrClamp01(d / 140.0f)) * 3.2f;
    h = fmaxf(h, landFloor);

    float rd = fminf(terrPolyDist(x, z, R1, 5), terrPolyDist(x, z, R2, 5));
    float channel = terrSmooth(terrClamp01(1.0f - rd / 38.0f));   // water channel
    float valley  = terrSmooth(terrClamp01(1.0f - rd / 150.0f));  // wider valley dip
    // Flood force fades upstream (east): near the coast the channel sinks below
    // water level (a navigable river), toward the foothills it only carves a dry
    // gorge — a mountain stream bed — so the range's foot never becomes a swamp.
    float flood = 1.0f - terrClamp01((x - 150.0f) / 500.0f);
    h -= valley * (2.5f + 3.5f * flood);
    h = h + (fminf(h, -2.2f) - h) * channel * 0.9f * flood;
    h -= channel * 5.0f * (1.0f - flood);
    if (flood < 0.6f) h = fmaxf(h, 0.6f);   // upstream beds stay dry land
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
        float h = fmaxf(0.0f, (broad + detail) * fmaxf(lane, edge));
        // Ballistics hill (west, behind the firing line): ~16 m terraced rise —
        // Paldiski's terraced swells in miniature. The flat ledges are known
        // shooter heights for comparing per-weapon drop/holdover on downhill
        // shots at the target wall (~60-90 m).
        float dw = sqrtf((x + 40.0f) * (x + 40.0f) + z * z);
        h += terrTerrace(terrSmooth(terrClamp01(1.0f - dw / 34.0f)) * 16.0f, 5.0f, 0.6f);
        // Target knoll (north-east, past the wall's end): a smooth bare slope
        // facing the pad, so uphill impact points read directly as drop at range.
        float dk = sqrtf((x - 40.0f) * (x - 40.0f) + (z - 42.0f) * (z - 42.0f));
        h += terrSmooth(terrClamp01(1.0f - dk / 30.0f)) * 9.0f;
        return h;
    }
    return 0.0f;
}
