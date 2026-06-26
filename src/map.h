#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>
#include "terrain.h"

// Static arena geometry, shared by server (collision) and client (collision + render).
// All boxes sit on the ground. Obstacle thickness stays >= 1.0 so bullets
// (max step ~0.85 m per physics tick) cannot tunnel through.
//
// Two maps exist: TRAINING (offline practice, symmetric cover) and WAREHOUSE
// (online matches, PUBG-style yard). The active map is chosen at runtime —
// server forces WAREHOUSE; the client uses TRAINING offline, WAREHOUSE once a
// server state arrives. Collision and render read the gMap* globals below.

struct Box {
    glm::vec3 center;  // y = half.y (resting on ground)
    glm::vec3 half;
};

enum MapId { MAP_TRAINING = 0, MAP_WAREHOUSE = 1, MAP_FIELD = 2, MAP_CITY = 3 };
constexpr int MAP_COUNT = 4;

// --- TRAINING: the original symmetric arena -------------------------------
inline const Box TRAINING_BOXES[] = {
    // center pillar
    {{ 0.0f, 1.5f,  0.0f}, {1.0f, 1.5f, 1.0f}},
    // walls on the axes
    {{ 6.0f, 1.0f,  0.0f}, {0.6f, 1.0f, 2.5f}},
    {{-6.0f, 1.0f,  0.0f}, {0.6f, 1.0f, 2.5f}},
    {{ 0.0f, 1.0f,  6.0f}, {2.5f, 1.0f, 0.6f}},
    {{ 0.0f, 1.0f, -6.0f}, {2.5f, 1.0f, 0.6f}},
    // crates on the diagonals (low — can be shot over)
    {{ 4.0f, 0.8f,  4.0f}, {0.8f, 0.8f, 0.8f}},
    {{-4.0f, 0.8f,  4.0f}, {0.8f, 0.8f, 0.8f}},
    {{ 4.0f, 0.8f, -4.0f}, {0.8f, 0.8f, 0.8f}},
    {{-4.0f, 0.8f, -4.0f}, {0.8f, 0.8f, 0.8f}},
    // tall pillars on the outer diagonals
    {{ 9.0f, 1.5f,  9.0f}, {0.7f, 1.5f, 0.7f}},
    {{-9.0f, 1.5f,  9.0f}, {0.7f, 1.5f, 0.7f}},
    {{ 9.0f, 1.5f, -9.0f}, {0.7f, 1.5f, 0.7f}},
    {{-9.0f, 1.5f, -9.0f}, {0.7f, 1.5f, 0.7f}},
    // shooting-range target wall (front face at x=25.4); a normal obstacle so the
    // shared collision/render handle it — no training-only physics path.
    {{26.0f, 4.0f,  0.0f}, {0.6f, 4.0f, 13.0f}},
};
inline constexpr int TRAINING_BOX_COUNT = (int)(sizeof(TRAINING_BOXES) / sizeof(TRAINING_BOXES[0]));

// --- WAREHOUSE: enclosed yard, central building, shipping containers -------
// Long axis is X (spawns sit at +/-15 X, facing center). Perimeter walls seal
// the play space; the central block and containers break sightlines.
inline const Box WAREHOUSE_BOXES[] = {
    // central two-story block (solid cover centerpiece)
    {{  0.0f, 2.5f,  0.0f}, {5.0f, 2.5f, 4.0f}},
    // perimeter walls (rectangle: X in [-22,22], Z in [-15,15])
    {{  0.0f, 1.5f,  15.0f}, {22.0f, 1.5f, 0.6f}},   // north
    {{  0.0f, 1.5f, -15.0f}, {22.0f, 1.5f, 0.6f}},   // south
    {{ 22.0f, 1.5f,  0.0f }, {0.6f,  1.5f, 15.0f}},  // east
    {{-22.0f, 1.5f,  0.0f }, {0.6f,  1.5f, 15.0f}},  // west
    // shipping containers (long on X)
    {{-13.0f, 1.25f,  8.0f}, {3.0f, 1.25f, 1.25f}},
    {{ 13.0f, 1.25f,  8.0f}, {3.0f, 1.25f, 1.25f}},
    {{-13.0f, 1.25f, -8.0f}, {3.0f, 1.25f, 1.25f}},
    {{ 13.0f, 1.25f, -8.0f}, {3.0f, 1.25f, 1.25f}},
    // shipping containers (long on Z, flanking the center)
    {{ -9.0f, 1.25f,  0.0f}, {1.25f, 1.25f, 3.0f}},
    {{  9.0f, 1.25f,  0.0f}, {1.25f, 1.25f, 3.0f}},
    // low wooden crates near the building (shoot over)
    {{ -5.0f, 0.75f,  10.0f}, {0.75f, 0.75f, 0.75f}},
    {{  5.0f, 0.75f,  10.0f}, {0.75f, 0.75f, 0.75f}},
    {{ -5.0f, 0.75f, -10.0f}, {0.75f, 0.75f, 0.75f}},
    {{  5.0f, 0.75f, -10.0f}, {0.75f, 0.75f, 0.75f}},
    // pipe/crate stacks by the end walls (near each spawn)
    {{-17.0f, 1.0f,  0.0f}, {1.0f, 1.0f, 2.0f}},
    {{ 17.0f, 1.0f,  0.0f}, {1.0f, 1.0f, 2.0f}},
};
inline constexpr int WAREHOUSE_BOX_COUNT = (int)(sizeof(WAREHOUSE_BOXES) / sizeof(WAREHOUSE_BOXES[0]));

// --- FIELD: 100 m^2 open terrain, no cover boxes ----------------------------
// Just the shared deterministic heightfield (see terrain.h). The same int-math
// noise runs on server + every client, so all players get one identical map with
// no asset load and nothing to sync over the network.
inline constexpr float FIELD_HALF = 50.0f;  // 100 m across

inline constexpr float ARENA_HALF = 45.0f;  // default hard clamp on X/Z (walls seal sooner)

// --- CITY: 300x300 m generated block grid (tier B: open-top wall shells) -----
// Deterministic generator runs identically on server + every client (pure int hash,
// no rand), so the city needs no asset load and nothing to sync. Buildings are
// four thin walls with a door gap on one side plus a little interior cover; flat
// ground, open tops (upper floors/roofs need step-up collision, not built yet).
inline constexpr float CITY_HALF  = 150.0f;  // 300 m across, clamp at +/-150
inline constexpr int   CITY_CELLS = 6;       // grid is CITY_CELLS x CITY_CELLS blocks
inline constexpr float CITY_CELL  = (2.0f * CITY_HALF) / CITY_CELLS;  // 50 m per block
inline constexpr int   MAX_CITY_BOXES = 1024;

// Per-box surface tag (decoupled from the renderer's MaterialId; material.h maps it).
enum BoxSurface : uint8_t { SURF_CONCRETE = 0, SURF_METAL, SURF_WOOD };

inline Box     gCityBoxes[MAX_CITY_BOXES];
inline uint8_t gCitySurf [MAX_CITY_BOXES];
inline int     gCityBoxCount = 0;
inline glm::vec3 gCitySpawns[64];
inline int       gCitySpawnCount = 0;

// Per-building metadata (client builds a textured facade mesh from this; the server
// ignores it and just uses the matching solid collision box). One box == one building.
struct CityBuilding {
    glm::vec3 center;        // footprint center, y = 0 (ground)
    float     w, d, h;       // full width (X), depth (Z), height (Y)
    int       baysX, baysZ;  // facade window columns per axis (texture tiles)
    int       floors;        // facade window rows (texture tiles vertically)
};
inline CityBuilding gCityBuildings[64];
inline int          gCityBuildingCount = 0;

inline uint32_t cityHash(int x, int z, uint32_t salt) {
    uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(z * 19349663) ^ (salt * 83492791u);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
inline float cityRand(int x, int z, uint32_t salt) {  // 0..1
    return (cityHash(x, z, salt) & 0xffffu) / 65535.0f;
}

inline void cityPush(const Box& b, uint8_t surf) {
    if (gCityBoxCount >= MAX_CITY_BOXES) return;
    gCityBoxes[gCityBoxCount] = b;
    gCitySurf [gCityBoxCount] = surf;
    gCityBoxCount++;
}

inline void generateCity() {
    gCityBoxCount      = 0;
    gCitySpawnCount    = 0;
    gCityBuildingCount = 0;
    const float FLOOR_M = 3.0f;   // meters per storey (facade window row)
    const float BAY_M   = 3.4f;   // meters per window column
    for (int cx = 0; cx < CITY_CELLS; cx++)
    for (int cz = 0; cz < CITY_CELLS; cz++) {
        float ccx = -CITY_HALF + CITY_CELL * (cx + 0.5f);
        float ccz = -CITY_HALF + CITY_CELL * (cz + 0.5f);
        // ~1 in 5 cells is an open plaza: sightlines + spawn room.
        if (cityHash(cx, cz, 7) % 5 == 0) {
            if (gCitySpawnCount < 64) gCitySpawns[gCitySpawnCount++] = {ccx, 0.0f, ccz};
            continue;
        }
        // Rectangular footprint: independent X/Z setback so blocks aren't all square.
        float mx = 7.0f + cityRand(cx, cz, 1) * 5.0f;       // street setback X (7..12 m)
        float mz = 7.0f + cityRand(cx, cz, 8) * 5.0f;       // street setback Z
        float hwx = CITY_CELL * 0.5f - mx;                  // half width  (X)
        float hwz = CITY_CELL * 0.5f - mz;                  // half depth  (Z)
        int   floors = 3 + (int)(cityHash(cx, cz, 2) % 5);  // 3..7 storeys
        float h  = floors * FLOOR_M;
        // Solid collision box (one per building); the facade mesh skins it client-side.
        cityPush({{ccx, h * 0.5f, ccz}, {hwx, h * 0.5f, hwz}}, SURF_CONCRETE);
        if (gCityBuildingCount < 64) {
            CityBuilding b;
            b.center = {ccx, 0.0f, ccz};
            b.w = 2 * hwx; b.d = 2 * hwz; b.h = h;
            b.baysX  = (int)fmaxf(2.0f, roundf(b.w / BAY_M));
            b.baysZ  = (int)fmaxf(2.0f, roundf(b.d / BAY_M));
            b.floors = floors;
            gCityBuildings[gCityBuildingCount++] = b;
        }
    }
    // Spawns at interior street intersections (always clear of footprints).
    for (int ix = 1; ix < CITY_CELLS && gCitySpawnCount < 64; ix++)
    for (int iz = 1; iz < CITY_CELLS && gCitySpawnCount < 64; iz++)
        gCitySpawns[gCitySpawnCount++] = {-CITY_HALF + CITY_CELL * ix, 0.0f,
                                          -CITY_HALF + CITY_CELL * iz};
}

// --- active map (runtime-selected; default training) ----------------------
inline const Box* gMapBoxes    = TRAINING_BOXES;
inline int        gMapBoxCount = TRAINING_BOX_COUNT;
inline MapId      gMapId       = MAP_TRAINING;
inline float      gArenaHalf   = ARENA_HALF;  // per-map clamp (grows for bigger maps)

// Half-extent (meters) the top-down map view / satellite texture is baked to:
// the whole *reachable* area, so the M map shows everything a player can occupy.
// WAREHOUSE is sealed by perimeter walls -> fit the farthest box edge. TRAINING is
// open ground out to the arena clamp; FIELD is the full 100 m clamp. Shared by the
// renderer's texture bake and the HUD full-map draw so image + overlays register.
inline float mapViewHalf() {
    if (gMapId == MAP_WAREHOUSE) {
        float ext = 5.0f;
        for (int i = 0; i < gMapBoxCount; i++) {
            const Box& b = gMapBoxes[i];
            ext = fmaxf(ext, fmaxf(fabsf(b.center.x) + b.half.x,
                                   fabsf(b.center.z) + b.half.z));
        }
        return ext * 1.06f;
    }
    return gArenaHalf;   // TRAINING: open clamp (45 m); FIELD: full 100 m
}

inline void setMap(MapId id) {
    gMapId = id;
    gTerrainOn = (id == MAP_FIELD);   // gates terrainHeight() in physics/spawns/render
    if (id == MAP_WAREHOUSE) {
        gMapBoxes = WAREHOUSE_BOXES; gMapBoxCount = WAREHOUSE_BOX_COUNT; gArenaHalf = 45.0f;
    } else if (id == MAP_FIELD) {
        gMapBoxes = nullptr;         gMapBoxCount = 0;                   gArenaHalf = FIELD_HALF;
    } else if (id == MAP_CITY) {
        generateCity();
        gMapBoxes = gCityBoxes;      gMapBoxCount = gCityBoxCount;       gArenaHalf = CITY_HALF;
    } else {
        gMapBoxes = TRAINING_BOXES;  gMapBoxCount = TRAINING_BOX_COUNT;  gArenaHalf = 45.0f;
    }
}
