#include "renderer.h"
#include "map.h"
#include "texture.h"
#include <cmath>
#include <vector>

static glm::vec3 linearColor(float r, float g, float b) {
    return {powf(r, 2.2f), powf(g, 2.2f), powf(b, 2.2f)};
}

static glm::vec3 wallColor(const KeilaBuilding& b, int index) {
    static const glm::vec3 house[] = {
        linearColor(0.78f, 0.74f, 0.66f), linearColor(0.70f, 0.68f, 0.64f),
        linearColor(0.80f, 0.78f, 0.72f), linearColor(0.66f, 0.60f, 0.52f),
        linearColor(0.72f, 0.62f, 0.50f), linearColor(0.58f, 0.62f, 0.56f)};
    static const glm::vec3 block[] = {
        linearColor(0.68f, 0.68f, 0.66f), linearColor(0.80f, 0.79f, 0.76f),
        linearColor(0.62f, 0.58f, 0.54f)};
    static const glm::vec3 shed[] = {
        linearColor(0.42f, 0.36f, 0.30f), linearColor(0.50f, 0.48f, 0.45f),
        linearColor(0.36f, 0.34f, 0.32f)};
    uint32_t h = mapHash(index, b.first, 17);
    if (b.kind == 1) return shed[h % 3];
    if (b.height > 11.0f) return block[h % 3];
    return house[h % 6];
}

static glm::vec3 roofColor(const KeilaBuilding& b, int index) {
    uint32_t h = mapHash(index, b.first, 23);
    if (b.kind == 0 && b.height <= 11.0f && h % 3 == 0) return linearColor(0.45f, 0.24f, 0.18f);
    return h % 2 ? linearColor(0.22f, 0.22f, 0.23f) : linearColor(0.30f, 0.29f, 0.28f);
}

static void pushVertex(std::vector<float>& v, float x, float y, float z,
                       const glm::vec3& n, const glm::vec3& c) {
    const float data[10] = {x, y, z, n.x, n.y, n.z, c.r, c.g, c.b, 0.0f};
    v.insert(v.end(), data, data + 10);
}

GLuint Renderer::keilaSurface() {
    if (!keilaSurfaceTried) {
        keilaSurfaceTried = true;
        keilaSurfaceTex = loadTexture("textures/keila_surface.png");
        if (keilaSurfaceTex) {
            glBindTexture(GL_TEXTURE_2D, keilaSurfaceTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
    return keilaSurfaceTex;
}

static GLuint loadClamped(const char* path) {
    GLuint tex = loadTexture(path);
    if (tex) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    return tex;
}

void Renderer::bindKeilaGround() {
    if (orthoNearLoc < 0) {
        orthoNearLoc = glGetUniformLocation(shader.program, "orthoNear");
        orthoFarLoc  = glGetUniformLocation(shader.program, "orthoFar");
        hasOrthoLoc  = glGetUniformLocation(shader.program, "hasOrtho");
        keilaOrthoNear = loadClamped("textures/keila_ortho_near.jpg");
        keilaOrthoFar  = loadClamped("textures/keila_ortho_far.jpg");
    }
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, keilaSurface());
    if (active == &shader) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, keilaOrthoNear);
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, keilaOrthoFar);
        shader.setInt(orthoNearLoc, 9);
        shader.setInt(orthoFarLoc, 10);
        shader.setInt(hasOrthoLoc, keilaOrthoNear && keilaOrthoFar ? 1 : 0);
    }
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::drawKeila(const Frustum& fr) {
    const int tiles = KEILA_TILES * KEILA_TILES;
    if (!keilaBuilt) {
        keilaBuilt = true;
        std::vector<float>    verts[KEILA_TILES * KEILA_TILES];
        std::vector<unsigned> index[KEILA_TILES * KEILA_TILES];
        const float tileSize = 2.0f * KEILA_WORLD_HALF / KEILA_TILES;
        for (int t = 0; t < tiles; t++) {
            keilaTileMin[t] = glm::vec3(1e9f);
            keilaTileMax[t] = glm::vec3(-1e9f);
        }
        for (int bi = 0; bi < KEILA_BUILDING_COUNT; bi++) {
            const KeilaBuilding& b = KEILA_BUILDINGS[bi];
            const float* p = KEILA_BUILDING_XZ + (size_t)b.first * 2;
            int tx = glm::clamp((int)((p[0] + KEILA_WORLD_HALF) / tileSize), 0, KEILA_TILES - 1);
            int tz = glm::clamp((int)((p[1] + KEILA_WORLD_HALF) / tileSize), 0, KEILA_TILES - 1);
            int t = tz * KEILA_TILES + tx;
            std::vector<float>& v = verts[t];
            std::vector<unsigned>& ix = index[t];
            float bottom = keilaBuildingBase(bi), top = keilaBuildingTop(bi);
            glm::vec3 wall = wallColor(b, bi), roof = roofColor(b, bi);
            for (int i = 0; i < b.count && b.kind != 2; i++) {
                int j = (i + 1) % b.count;
                float ax = p[i * 2], az = p[i * 2 + 1], bx = p[j * 2], bz = p[j * 2 + 1];
                float dx = bx - ax, dz = bz - az, len = sqrtf(dx * dx + dz * dz);
                if (len < 1e-4f) continue;
                glm::vec3 n(dz / len, 0.0f, -dx / len);
                unsigned base = (unsigned)(v.size() / 10);
                pushVertex(v, ax, bottom, az, n, wall);
                pushVertex(v, bx, bottom, bz, n, wall);
                pushVertex(v, bx, top, bz, n, wall);
                pushVertex(v, ax, top, az, n, wall);
                const unsigned quad[6] = {base, base + 2, base + 1, base, base + 3, base + 2};
                ix.insert(ix.end(), quad, quad + 6);
            }
            for (int i = 0; i < b.count; i++) {
                keilaTileMin[t] = glm::min(keilaTileMin[t], glm::vec3(p[i * 2], bottom, p[i * 2 + 1]));
                keilaTileMax[t] = glm::max(keilaTileMax[t], glm::vec3(p[i * 2], top, p[i * 2 + 1]));
            }
            unsigned base = (unsigned)(v.size() / 10);
            for (int i = 0; i < b.count; i++)
                pushVertex(v, p[i * 2], top, p[i * 2 + 1], glm::vec3(0, 1, 0), roof);
            const uint16_t* tri = KEILA_ROOF_INDEX + b.roofFirst;
            for (int i = 0; i + 2 < b.roofCount; i += 3) {
                ix.push_back(base + tri[i]);
                ix.push_back(base + tri[i + 2]);
                ix.push_back(base + tri[i + 1]);
            }
        }
        for (int t = 0; t < tiles; t++)
            if (!index[t].empty())
                keilaTile[t].create(verts[t].data(), verts[t].size(), index[t].data(),
                                    index[t].size(), true, false, true);
        buildKeilaLandmarks();
    }
    drawKeilaLandmarks(fr);
    for (int t = 0; t < tiles; t++) {
        if (!keilaTile[t].indexCount) continue;
        glm::vec3 center = (keilaTileMin[t] + keilaTileMax[t]) * 0.5f;
        glm::vec3 half   = (keilaTileMax[t] - keilaTileMin[t]) * 0.5f;
        if (!fr.aabbVisible(center, half)) continue;
        drawMeshModel(keilaTile[t], glm::mat4(1.0f), glm::vec3(1.0f));
    }
}

void Renderer::destroyKeila() {
    for (Mesh& m : keilaTile) m.destroy();
    for (Mesh& m : keilaLandmark) m.destroy();
    for (GLuint& t : keilaLandmarkTex) {
        if (t) glDeleteTextures(1, &t);
        t = 0;
    }
    keilaBuilt = false;
    if (keilaOrthoNear) glDeleteTextures(1, &keilaOrthoNear);
    if (keilaOrthoFar) glDeleteTextures(1, &keilaOrthoFar);
    keilaOrthoNear = keilaOrthoFar = 0;
    orthoNearLoc = -1;
    if (keilaSurfaceTex) glDeleteTextures(1, &keilaSurfaceTex);
    keilaSurfaceTex = 0;
    keilaSurfaceTried = false;
}
