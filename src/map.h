#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "terrain.h"

// Static map geometry, shared by server (collision) and client (collision + render).
// One map exists today: PALDISKI, a 500x500 m Estonian coastal town — sea on the
// west, a shore town of Soviet apartment slabs, hills inland. Everything is generated
// deterministically (pure int-hash noise), so server + every client build the same
// world with no assets and nothing to sync.
//
// The MapId enum / setMap() / mapFromName() registry stays in place so future maps
// slot in: add an id, a generator, a name, and a setMap branch.

struct Box {
    glm::vec3 center;
    glm::vec3 half;
};

// MAP_LOBBY is the client-only offline sandbox (flat pad, shooting range, test
// cover) you sit in until you press C and pick a server. It is NOT a server map:
// mapFromName never returns it and no server runs it — only real maps precede it.
enum MapId { MAP_PALDISKI = 0, MAP_LOBBY, MAP_COUNT_ };
constexpr int MAP_COUNT = MAP_COUNT_;   // sizes client arrays (incl. the lobby)

inline constexpr float PALDISKI_HALF = 250.0f;   // 500 m across, clamp at +/-250
inline constexpr float SEA_LEVEL     = 0.0f;     // water plane height
inline constexpr int   MAX_MAP_BOXES = 1024;

// Per-box surface tag (decoupled from the renderer's MaterialId; material.h maps it).
enum BoxSurface : uint8_t { SURF_CONCRETE = 0, SURF_METAL, SURF_WOOD };

inline Box       gTownBoxes[MAX_MAP_BOXES];
inline uint8_t   gTownSurf [MAX_MAP_BOXES];
inline int       gTownBoxCount = 0;
inline glm::vec3 gMapSpawns[64];
inline int       gMapSpawnCount = 0;

// Per-building metadata (client builds a textured facade mesh from this; the server
// ignores it and just uses the matching solid collision box). One box == one building.
// center.y is the pad height the building's base sits at.
struct TownBuilding {
    glm::vec3 center;        // footprint center, y = pad/base height
    float     w, d, h;       // full width (X), depth (Z), height (Y)
    int       baysX, baysZ;  // facade window columns per axis (texture tiles)
    int       floors;        // facade window rows (texture tiles vertically)
};
inline TownBuilding gTownBuildings[64];
inline int          gTownBuildingCount = 0;

inline uint32_t mapHash(int x, int z, uint32_t salt) {
    uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(z * 19349663) ^ (salt * 83492791u);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
inline float mapRand(int x, int z, uint32_t salt) {  // 0..1
    return (mapHash(x, z, salt) & 0xffffu) / 65535.0f;
}

inline void townPush(const Box& b, uint8_t surf) {
    if (gTownBoxCount >= MAX_MAP_BOXES) return;
    gTownBoxes[gTownBoxCount] = b;
    gTownSurf [gTownBoxCount] = surf;
    gTownBoxCount++;
}

// Build the Paldiski shore town: a row of Plattenbau slabs hugging the coast on
// flattened terrain pads, plus spawn points on safe ground. Deterministic — the
// same world on server and every client.
inline void generatePaldiski() {
    gTownBoxCount      = 0;
    gTownBuildingCount = 0;
    gMapSpawnCount     = 0;
    gTerrainPadCount   = 0;

    const float FLOOR_M = 3.0f;   // meters per storey (facade window row)
    const float BAY_M   = 3.4f;   // meters per window column

    // Six slabs marching down the coast, each ~40 m apart on Z, set back from the
    // waterline. Pad height comes from the raw terrain, clamped above the beach.
    for (int i = 0; i < 6; i++) {
        float bz = -100.0f + i * 42.0f + (mapRand(i, 3, 21) - 0.5f) * 14.0f;
        float bx = paldiskiShoreX(bz) + 52.0f + mapRand(i, 5, 22) * 18.0f;
        float padH = fmaxf(paldiskiBase(bx, bz), 2.5f);

        float w = 24.0f + mapRand(i, 1, 23) * 10.0f;    // 24..34 m long (X)
        float d = 10.0f + mapRand(i, 2, 24) * 2.5f;     // 10..12.5 m deep (Z)
        int   floors = 4 + (int)(mapHash(i, 4, 25) % 2); // 4..5 storeys
        float h = floors * FLOOR_M;

        // Radius: footprint half-diagonal + corner props must fit inside the flat
        // plateau (r minus the 12 m falloff skirt — see paldiskiElevation).
        float halfDiag = sqrtf(w * w + d * d) * 0.5f;
        if (gTerrainPadCount < MAX_TERRAIN_PADS)
            gTerrainPads[gTerrainPadCount++] = {bx, bz, halfDiag + 3.0f + 12.0f, padH};

        townPush({{bx, padH + h * 0.5f, bz}, {w * 0.5f, h * 0.5f, d * 0.5f}}, SURF_CONCRETE);
        if (gTownBuildingCount < 64) {
            TownBuilding b;
            b.center = {bx, padH, bz};
            b.w = w; b.d = d; b.h = h;
            b.baysX  = (int)fmaxf(2.0f, roundf(w / BAY_M));
            b.baysZ  = (int)fmaxf(2.0f, roundf(d / BAY_M));
            b.floors = floors;
            gTownBuildings[gTownBuildingCount++] = b;
        }
    }

    // Spawns: the yard in front of each building (toward the sea) + an inland arc
    // in the hills, all on ground safely above the waterline.
    for (int i = 0; i < gTownBuildingCount; i++) {
        const TownBuilding& b = gTownBuildings[i];
        gMapSpawns[gMapSpawnCount++] = {b.center.x - b.w * 0.5f - 8.0f, 0.0f, b.center.z};
    }
    for (int i = 0; i < 10 && gMapSpawnCount < 64; i++) {
        float sz = -180.0f + i * 40.0f;
        float sx = 60.0f + mapRand(i, 9, 26) * 120.0f;
        if (paldiskiElevation(sx, sz) < 1.0f) sx += 60.0f;   // nudge off any low spot
        gMapSpawns[gMapSpawnCount++] = {sx, 0.0f, sz};
    }
}

// Build the offline lobby: a flat 120x120 m pad with a shooting-range target wall
// (front face at x=25.4, matching the aim cross drawn client-side), a little test
// cover, and the practice dummy's yard. Reuses the shared box/spawn arrays so
// collision, materials, and the minimap bake all work unchanged.
inline constexpr float LOBBY_HALF        = 60.0f;
inline constexpr float LOBBY_WALL_FACE   = 25.4f;  // target wall front plane (x)
inline constexpr float LOBBY_BULLSEYE_Y  = 1.7f;   // aim-cross height (standing eyes)

inline void generateLobby() {
    gTownBoxCount      = 0;
    gTownBuildingCount = 0;   // no facade buildings in the lobby
    gMapSpawnCount     = 0;
    gTerrainPadCount   = 0;
    // target wall
    townPush({{26.0f, 4.0f,  0.0f}, {0.6f, 4.0f, 13.0f}}, SURF_CONCRETE);
    // test cover: pillar, low crates (shoot over), a metal container to lean around
    townPush({{ 0.0f, 1.5f, -8.0f}, {1.0f, 1.5f, 1.0f}}, SURF_CONCRETE);
    townPush({{ 6.0f, 0.8f,  8.0f}, {0.8f, 0.8f, 0.8f}}, SURF_WOOD);
    townPush({{ 9.0f, 0.8f,  8.0f}, {0.8f, 0.8f, 0.8f}}, SURF_WOOD);
    townPush({{-8.0f, 1.25f, 6.0f}, {3.0f, 1.25f, 1.25f}}, SURF_METAL);
    gMapSpawns[gMapSpawnCount++] = {12.0f, 0.0f, 0.0f};   // firing line, faces the wall
}

// --- active map (runtime-selected) -----------------------------------------
inline const Box* gMapBoxes    = nullptr;
inline int        gMapBoxCount = 0;
inline MapId      gMapId       = MAP_PALDISKI;
inline float      gArenaHalf   = PALDISKI_HALF;   // hard clamp on X/Z

// Half-extent (meters) the top-down map view / satellite texture covers. Shared by
// the renderer's texture bake and the HUD full-map draw so image + overlays register.
inline float mapViewHalf() { return gArenaHalf; }

// Parse an FPS_MAP env string to a MapId. Shared by server + client so both honor
// the same names. Returns `fallback` for null/unknown. One name today; future maps
// add theirs here.
inline MapId mapFromName(const char* s, MapId fallback) {
    if (!s) return fallback;
    if (strcmp(s, "paldiski") == 0) return MAP_PALDISKI;
    return fallback;
}

inline void setMap(MapId id) {
    gMapId = id;
    // Future maps add their generator + clamp + terrain mode as branches here.
    if (id == MAP_LOBBY) {
        generateLobby();
        gTerrainMode = TERRAIN_LOBBY;
        gArenaHalf   = LOBBY_HALF;
    } else {
        generatePaldiski();
        gTerrainMode = TERRAIN_PALDISKI;
        gArenaHalf   = PALDISKI_HALF;
    }
    gMapBoxes    = gTownBoxes;
    gMapBoxCount = gTownBoxCount;
}
