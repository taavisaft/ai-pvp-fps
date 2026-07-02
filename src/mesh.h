#pragma once
#include <cstddef>
#include "gl_loader.h"

struct Mesh {
    GLuint  vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;

    // verts: xyz triples (or xyz+normal 6-tuples if withNormals, or xyz+normal+uv
    // 8-tuples if withUV). floatCount = total floats. withNormals enables attrib 1
    // (normal); withUV additionally enables attrib 2 (uv) with a stride-8 layout.
    bool create(const float* verts, size_t floatCount,
                const unsigned* indices, size_t idxCount,
                bool withNormals = false, bool withUV = false);
    void draw() const;
    void destroy();
};

// Builders for the game's fixed geometry
bool createUnitCube(Mesh& m);      // 1x1x1 centered at origin
bool createGroundQuad(Mesh& m);    // 100x100 at y=0 centered at origin
bool createQuad2D(Mesh& m);        // 1x1 in XY plane at z=0, facing +z (HUD)
// Heightfield grid spanning +/-half, sampled from `elev` (field or lobby ground).
bool createTerrainMesh(Mesh& m, float half, float (*elev)(float, float));
