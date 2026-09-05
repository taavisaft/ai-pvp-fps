#include "mesh.h"
#include "map.h"      // FIELD_HALF + terrain.h (terrainElevation)
#include <vector>

bool Mesh::create(const float* verts, size_t floatCount,
                  const unsigned* indices, size_t idxCount, bool withNormals, bool withUV, bool withMaterial) {
    authoredMaterial = withMaterial;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    if (!vao || !vbo || !ebo) return false;

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, floatCount * sizeof(float), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxCount * sizeof(unsigned), indices, GL_STATIC_DRAW);

    // withUV implies pos+normal+uv (stride 8); withNormals = pos+normal (6); else pos (3).
    int comps = withMaterial ? 10 : (withUV ? 8 : (withNormals ? 6 : 3));
    GLsizei stride = comps * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    if (withNormals || withUV || withMaterial) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    }
    if (withUV && !withMaterial) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    }

    if (withMaterial) {
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    }
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

// Heightfield grid spanning +/-half, sampled from `elev`. Built once (static VBO)
// with analytic per-vertex normals (central difference of the height field) so the
// hills shade smoothly and self-shadow, instead of faceting from dFdx/dFdy.
// Interleaved pos.xyz, normal.xyz.
bool createTerrainMesh(Mesh& m, float half, float (*elev)(float, float)) {
    const float STEP = 2.0f;
    const int   N = (int)(2.0f * half / STEP) + 1;   // verts per side
    const float e = STEP;                            // gradient sample spacing
    std::vector<float> v;
    v.reserve((size_t)N * N * 6);
    for (int zi = 0; zi < N; zi++)
        for (int xi = 0; xi < N; xi++) {
            float x = -half + xi * STEP, z = -half + zi * STEP;
            float dhdx = (elev(x + e, z) - elev(x - e, z)) / (2 * e);
            float dhdz = (elev(x, z + e) - elev(x, z - e)) / (2 * e);
            glm::vec3 n = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
            v.push_back(x);
            v.push_back(elev(x, z));
            v.push_back(z);
            v.push_back(n.x);
            v.push_back(n.y);
            v.push_back(n.z);
        }
    std::vector<unsigned> idx;
    idx.reserve((size_t)(N - 1) * (N - 1) * 6);
    for (int zi = 0; zi < N - 1; zi++)
        for (int xi = 0; xi < N - 1; xi++) {
            unsigned a = zi * N + xi, b = a + 1, c = a + N, d = c + 1;
            idx.push_back(a); idx.push_back(c); idx.push_back(d);  // up-facing CCW
            idx.push_back(d); idx.push_back(b); idx.push_back(a);
        }
    return m.create(v.data(), v.size(), idx.data(), idx.size(), /*withNormals=*/true);
}

bool createTerrainPatch(Mesh& m, float x0, float z0, float sizeX, float sizeZ,
                        float step, float skirt, float (*elev)(float, float),
                        float* outMinY, float* outMaxY) {
    const int NX = (int)(sizeX / step) + 1;   // verts per side (x)
    const int NZ = (int)(sizeZ / step) + 1;   // verts per side (z)
    // Sample the height grid ONCE with a one-cell border, then derive normals from
    // neighbours — 1 elev() call per grid point instead of 5 (chunk builds happen
    // lazily at runtime, so build cost is frame-hitch budget).
    std::vector<float> hg((size_t)(NZ + 2) * (NX + 2));
    for (int zi = -1; zi <= NZ; zi++)
        for (int xi = -1; xi <= NX; xi++)
            hg[(size_t)(zi + 1) * (NX + 2) + (xi + 1)] =
                elev(x0 + xi * step, z0 + zi * step);
    auto H = [&](int xi, int zi) { return hg[(size_t)(zi + 1) * (NX + 2) + (xi + 1)]; };

    float minY = 1e9f, maxY = -1e9f;
    std::vector<float> v;
    v.reserve((size_t)NX * NZ * 6 + (size_t)(NX + NZ) * 2 * 6);
    for (int zi = 0; zi < NZ; zi++)
        for (int xi = 0; xi < NX; xi++) {
            float x = x0 + xi * step, z = z0 + zi * step;
            float h = H(xi, zi);
            float dhdx = (H(xi + 1, zi) - H(xi - 1, zi)) / (2 * step);
            float dhdz = (H(xi, zi + 1) - H(xi, zi - 1)) / (2 * step);
            glm::vec3 n = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
            v.insert(v.end(), {x, h, z, n.x, n.y, n.z});
            if (h < minY) minY = h;
            if (h > maxY) maxY = h;
        }
    std::vector<unsigned> idx;
    idx.reserve((size_t)(NX - 1) * (NZ - 1) * 6);
    for (int zi = 0; zi < NZ - 1; zi++)
        for (int xi = 0; xi < NX - 1; xi++) {
            unsigned a = zi * NX + xi, b = a + 1, c = a + NX, d = c + 1;
            idx.insert(idx.end(), {a, c, d, d, b, a});
        }
    // Skirt: duplicate each boundary vertex sunk by `skirt`, stitched with quads
    // along all four edges. Hides sub-`skirt` cracks at LOD boundaries.
    if (skirt > 0.0f) {
        auto addSkirtRun = [&](int count, unsigned (*indexAt)(int, int, int), bool flip) {
            unsigned prevTop = 0, prevBot = 0;
            for (int i = 0; i < count; i++) {
                unsigned top = indexAt(i, NX, NZ);
                unsigned bot = (unsigned)(v.size() / 6);
                v.insert(v.end(), {v[top * 6 + 0], v[top * 6 + 1] - skirt, v[top * 6 + 2],
                                   v[top * 6 + 3], v[top * 6 + 4], v[top * 6 + 5]});
                if (i > 0) {
                    if (flip) idx.insert(idx.end(), {prevTop, prevBot, bot, bot, top, prevTop});
                    else      idx.insert(idx.end(), {prevTop, top, bot, bot, prevBot, prevTop});
                }
                prevTop = top; prevBot = bot;
            }
        };
        addSkirtRun(NX, [](int i, int nx, int nz) { return (unsigned)i; }, false);
        addSkirtRun(NX, [](int i, int nx, int nz) { return (unsigned)((nz - 1) * nx + i); }, true);
        addSkirtRun(NZ, [](int i, int nx, int nz) { return (unsigned)(i * nx); }, true);
        addSkirtRun(NZ, [](int i, int nx, int nz) { return (unsigned)(i * nx + nx - 1); }, false);
    }
    if (outMinY) *outMinY = minY;
    if (outMaxY) *outMaxY = maxY;
    return m.create(v.data(), v.size(), idx.data(), idx.size(), /*withNormals=*/true);
}
