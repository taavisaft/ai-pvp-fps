#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "terrain.h"
#include "stand_col.h"

// Static map geometry, shared by server (collision) and client (collision + render).
// One map exists today: PALDISKI, a 500x500 m Estonian coastal town — sea on the
// west, a shore town of Soviet apartment slabs, hills inland. Everything is generated
// deterministically (pure int-hash noise), so server + every client build the same
// world with no assets and nothing to sync.
//
// The MapId enum / setMap() / mapFromName() registry stays in place so future maps
// slot in: add an id, a generator, a name, and a setMap branch.

// Defined in tree_scatter.cpp (shared): rebuild the deterministic tree scatter +
// collision cylinders for the active map. Called at the end of setMap so server and
// client agree without syncing. Declared (not included) to keep this header light.
void buildTreeColliders();

struct Box {
    glm::vec3 center;
    glm::vec3 half;
};

// MAP_LOBBY is the client-only offline sandbox (flat pad, shooting range, test
// cover) you sit in until you press C and pick a server. It is NOT a server map:
// mapFromName never returns it and no server runs it — only real maps precede it.
enum MapId { MAP_PALDISKI = 0, MAP_LOBBY, MAP_COUNT_ };
constexpr int MAP_COUNT = MAP_COUNT_;   // sizes client arrays (incl. the lobby)

inline constexpr float PALDISKI_HALF = 1024.0f;  // 2x2 km play area, clamp at +/-1024
inline constexpr float SEA_LEVEL     = 0.0f;     // water plane height
inline constexpr int   MAX_MAP_BOXES = 1024;

// Per-box surface tag (decoupled from the renderer's MaterialId; material.h maps it).
enum BoxSurface : uint8_t { SURF_CONCRETE = 0, SURF_METAL, SURF_WOOD };

inline Box       gTownBoxes[MAX_MAP_BOXES];
inline uint8_t   gTownSurf [MAX_MAP_BOXES];
inline int       gTownBoxCount = 0;
inline glm::vec3 gMapSpawns[64];
inline int       gMapSpawnCount = 0;

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

// Paldiski is bare terrain for now — the town/props were reset alongside the
// vegetation to rebuild the world terrain-first (structures return later, on the
// finished ground). Only spawn points are generated: a deterministic scatter on
// safe land above the waterline.
inline void generatePaldiski() {
    gTownBoxCount    = 0;
    gMapSpawnCount   = 0;
    gTerrainPadCount = 0;

    // Primary test/player-0 spawn: a broad hilltop (~41 m) with a gentle local
    // slope and long views toward the central valleys. Keeping it first makes both
    // FPS_MAP=paldiski practice and the first online join start on high ground.
    gMapSpawns[gMapSpawnCount++] = {-301.0f, 0.0f, 527.0f};

    for (int i = 0; i < 24 && gMapSpawnCount < 64; i++) {
        // Scatter across the taiga between shore and foothills; skip water/rivers.
        float sz = -850.0f + (i % 12) * 150.0f + (mapRand(i, 4, 27) - 0.5f) * 90.0f;
        float sx = -450.0f + mapRand(i, 9, 26) * 1050.0f;
        for (int tries = 0; tries < 4 && paldiskiElevation(sx, sz) < 1.0f; tries++)
            sx += 90.0f;   // walk east until on dry land
        if (paldiskiElevation(sx, sz) < 1.0f) continue;
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
inline constexpr float LOBBY_STAND_X     = -6.0f;
inline constexpr float LOBBY_STAND_Z     = -13.0f;

inline int gStandBoxFirst = 0;
inline int gStandBoxCount = 0;

inline void generateLobby() {
    gTownBoxCount      = 0;
    gMapSpawnCount     = 0;
    gTerrainPadCount   = 0;
    // target wall
    townPush({{26.0f, 4.0f,  0.0f}, {0.6f, 4.0f, 13.0f}}, SURF_CONCRETE);
    // test cover: pillar, low crates (shoot over), a metal container to lean around
    townPush({{ 0.0f, 1.5f, -8.0f}, {1.0f, 1.5f, 1.0f}}, SURF_CONCRETE);
    townPush({{ 6.0f, 0.8f,  8.0f}, {0.8f, 0.8f, 0.8f}}, SURF_WOOD);
    townPush({{ 9.0f, 0.8f,  8.0f}, {0.8f, 0.8f, 0.8f}}, SURF_WOOD);
    townPush({{-8.0f, 1.25f, 6.0f}, {3.0f, 1.25f, 1.25f}}, SURF_METAL);
    float sy = terrainHeight(LOBBY_STAND_X, LOBBY_STAND_Z);
    gStandBoxFirst = gTownBoxCount;
    for (int i = 0; i < STAND_COL_COUNT; i++) {
        const float* c = STAND_COL[i];
        townPush({{LOBBY_STAND_X + c[0], sy + c[1], LOBBY_STAND_Z + c[2]},
                  {c[3], c[4], c[5]}}, SURF_WOOD);
    }
    gStandBoxCount = gTownBoxCount - gStandBoxFirst;
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
        gTerrainMode = TERRAIN_LOBBY;
        gArenaHalf   = LOBBY_HALF;
        generateLobby();
    } else {
        gTerrainMode = TERRAIN_PALDISKI;
        gArenaHalf   = PALDISKI_HALF;
        generatePaldiski();
    }
    gMapBoxes    = gTownBoxes;
    gMapBoxCount = gTownBoxCount;
    buildTreeColliders();   // deterministic tree scatter for server + client collision
}
