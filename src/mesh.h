#pragma once
#include <cstddef>
#include "gl_loader.h"

struct Mesh {
    GLuint  vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;

    // verts: xyz triples, floatCount = total floats (3 per vertex)
    bool create(const float* verts, size_t floatCount,
                const unsigned* indices, size_t idxCount);
    void draw() const;
    void destroy();
};

// Builders for the game's fixed geometry
bool createUnitCube(Mesh& m);      // 1x1x1 centered at origin
bool createGroundQuad(Mesh& m);    // 100x100 at y=0 centered at origin
bool createQuad2D(Mesh& m);        // 1x1 in XY plane at z=0, facing +z (HUD)
bool createTerrainMesh(Mesh& m);   // 1 km^2 heightfield grid (MAP_FIELD)
