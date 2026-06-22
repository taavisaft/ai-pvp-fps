#pragma once
#include <glm/glm.hpp>
#include <cmath>
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

enum MapId { MAP_TRAINING = 0, MAP_WAREHOUSE = 1, MAP_FIELD = 2 };

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

// --- FIELD: 1 km^2 open terrain, no cover boxes -----------------------------
// Just the shared deterministic heightfield (see terrain.h). The same int-math
// noise runs on server + every client, so all players get one identical map with
// no asset load and nothing to sync over the network.
inline constexpr float FIELD_HALF = 500.0f;  // 1 km across

inline constexpr float ARENA_HALF = 45.0f;  // default hard clamp on X/Z (walls seal sooner)

// --- active map (runtime-selected; default training) ----------------------
inline const Box* gMapBoxes    = TRAINING_BOXES;
inline int        gMapBoxCount = TRAINING_BOX_COUNT;
inline MapId      gMapId       = MAP_TRAINING;
inline float      gArenaHalf   = ARENA_HALF;  // per-map clamp (grows for bigger maps)

// Half-extent (meters) the top-down map view / satellite texture is baked to:
// the farthest box edge from origin plus a small margin. Shared by the renderer's
// map-texture bake and the HUD full-map draw so the image and overlays register.
inline float mapViewHalf() {
    float ext = 5.0f;
    for (int i = 0; i < gMapBoxCount; i++) {
        const Box& b = gMapBoxes[i];
        ext = fmaxf(ext, fmaxf(fabsf(b.center.x) + b.half.x,
                               fabsf(b.center.z) + b.half.z));
    }
    return gMapBoxCount > 0 ? ext * 1.06f : 120.0f;  // FIELD has no boxes -> local window
}

inline void setMap(MapId id) {
    gMapId = id;
    gTerrainOn = (id == MAP_FIELD);   // gates terrainHeight() in physics/spawns/render
    if (id == MAP_WAREHOUSE) {
        gMapBoxes = WAREHOUSE_BOXES; gMapBoxCount = WAREHOUSE_BOX_COUNT; gArenaHalf = 45.0f;
    } else if (id == MAP_FIELD) {
        gMapBoxes = nullptr;         gMapBoxCount = 0;                   gArenaHalf = FIELD_HALF;
    } else {
        gMapBoxes = TRAINING_BOXES;  gMapBoxCount = TRAINING_BOX_COUNT;  gArenaHalf = 45.0f;
    }
}
