#pragma once
#include <glm/glm.hpp>
#include "mesh.h"
#include "frustum.h"

// Chunked LOD terrain renderer for the 2x2 km taiga map, plus the non-walkable
// mountain vista beyond the play boundary.
//
// The play area is split into CHUNKS x CHUNKS square chunks; each lazily builds up
// to three meshes of the same ground at different grid steps and draws the tier for
// its camera distance. Edge skirts hide the sub-meter cracks at LOD boundaries.
// Heights come from the shared analytic terrainHeight(), so collision is exact
// everywhere regardless of render tessellation.
//
// Two anti-shimmer rules learned the hard way:
//  - The vista is FOUR slabs that exclude the play area entirely (no coarse mesh
//    hiding under the fine chunks -> nothing to z-fight at long range).
//  - Expensive chunk builds are budgeted to one per frame; until a chunk's ideal
//    LOD exists it draws its best already-built tier (or the cheap coarse one,
//    built instantly) instead of stalling the frame.
struct TerrainChunks {
    static constexpr int   CHUNKS     = 16;      // per axis
    static constexpr float CHUNK_SIZE = 128.0f;  // meters (16 x 128 = 2048)
    static constexpr int   LODS       = 3;
    static constexpr float LOD_STEP[LODS] = {2.0f, 8.0f, 32.0f};
    static constexpr float LOD_DIST[LODS] = {400.0f, 1000.0f, 1e9f};
    static constexpr float VISTA_HALF     = 4096.0f;
    static constexpr float VISTA_STEP     = 64.0f;

    struct Chunk {
        Mesh  lod[LODS];
        bool  built[LODS] = {false, false, false};
        float minY = 0.0f, maxY = 0.0f;
        bool  boundsKnown = false;
    };
    Chunk chunks[CHUNKS][CHUNKS];
    Mesh  vistaSlab[4];
    bool  vistaBuilt = false;
    int   maxBuildsPerFrame = 1;   // expensive LOD mesh builds budget (quality tier)

    // Draw all visible chunks (+ vista) with whatever program is already set up —
    // the caller (Renderer::drawTerrain) binds textures/uniforms first.
    void draw(const Frustum& fr, const glm::vec3& eye, bool withVista);
    void destroy();
};
