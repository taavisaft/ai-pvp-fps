#include "mesh.h"
#include "map.h"      // FIELD_HALF + terrain.h (terrainElevation)
#include <vector>

bool Mesh::create(const float* verts, size_t floatCount,
                  const unsigned* indices, size_t idxCount) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    if (!vao || !vbo || !ebo) return false;

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, floatCount * sizeof(float), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxCount * sizeof(unsigned), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
    indexCount = (GLsizei)idxCount;
    return true;
}

void Mesh::draw() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

void Mesh::destroy() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = 0;
    indexCount = 0;
}

bool createUnitCube(Mesh& m) {
    static const float verts[] = {
        -0.5f, -0.5f, -0.5f,   // 0
         0.5f, -0.5f, -0.5f,   // 1
         0.5f,  0.5f, -0.5f,   // 2
        -0.5f,  0.5f, -0.5f,   // 3
        -0.5f, -0.5f,  0.5f,   // 4
         0.5f, -0.5f,  0.5f,   // 5
         0.5f,  0.5f,  0.5f,   // 6
        -0.5f,  0.5f,  0.5f,   // 7
    };
    // CCW winding viewed from outside (GL_CULL_FACE is on)
    static const unsigned idx[] = {
        4, 5, 6,  6, 7, 4,   // +z
        1, 0, 3,  3, 2, 1,   // -z
        0, 4, 7,  7, 3, 0,   // -x
        5, 1, 2,  2, 6, 5,   // +x
        7, 6, 2,  2, 3, 7,   // +y
        0, 1, 5,  5, 4, 0,   // -y
    };
    return m.create(verts, 24, idx, 36);
}

bool createQuad2D(Mesh& m) {
    static const float verts[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f,
    };
    static const unsigned idx[] = { 0, 1, 2,  2, 3, 0 };  // CCW in screen space
    return m.create(verts, 12, idx, 6);
}

bool createGroundQuad(Mesh& m) {
    static const float verts[] = {
        -50.0f, 0.0f, -50.0f,  // 0
         50.0f, 0.0f, -50.0f,  // 1
         50.0f, 0.0f,  50.0f,  // 2
        -50.0f, 0.0f,  50.0f,  // 3
    };
    static const unsigned idx[] = { 0, 3, 2,  2, 1, 0 };  // up-facing CCW
    return m.create(verts, 12, idx, 6);
}

// Heightfield grid spanning the field map, sampled from terrainElevation(). Built
// once (static VBO); the shader derives normals via dFdx/dFdy, so position-only.
bool createTerrainMesh(Mesh& m) {
    const float HALF = FIELD_HALF, STEP = 4.0f;
    const int   N = (int)(2.0f * HALF / STEP) + 1;   // verts per side
    std::vector<float> v;
    v.reserve((size_t)N * N * 3);
    for (int zi = 0; zi < N; zi++)
        for (int xi = 0; xi < N; xi++) {
            float x = -HALF + xi * STEP, z = -HALF + zi * STEP;
            v.push_back(x);
            v.push_back(terrainElevation(x, z));
            v.push_back(z);
        }
    std::vector<unsigned> idx;
    idx.reserve((size_t)(N - 1) * (N - 1) * 6);
    for (int zi = 0; zi < N - 1; zi++)
        for (int xi = 0; xi < N - 1; xi++) {
            unsigned a = zi * N + xi, b = a + 1, c = a + N, d = c + 1;
            idx.push_back(a); idx.push_back(c); idx.push_back(d);  // up-facing CCW
            idx.push_back(d); idx.push_back(b); idx.push_back(a);
        }
    return m.create(v.data(), v.size(), idx.data(), idx.size());
}
