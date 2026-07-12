#include "props.h"
#include "renderer.h"   // full Renderer type (lighting palette + shadow map)
#include "map.h"         // PALDISKI_HALF, gTownBuildings, mapHash/mapRand, terrainHeight
#include <SDL.h>
#include <cstdio>
#include <cfloat>

namespace {
// Append an axis-aligned box (6 quad faces, outward CCW winding) into the vertex/index
// soup. Vertex = pos.xyz, normal.xyz, color.xyz (9 floats). Tangent pairs are chosen so
// u x v == n, giving front-facing triangles for GL_CULL_FACE.
void appendBox(std::vector<float>& V, std::vector<unsigned>& I,
               glm::vec3 c, glm::vec3 h, glm::vec3 col) {
    const glm::vec3 N[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const glm::vec3 U[6] = {{0,1,0},{0,0,1},{0,0,1},{1,0,0},{1,0,0},{0,1,0}};
    const glm::vec3 Vt[6]= {{0,0,1},{0,1,0},{1,0,0},{0,0,1},{0,1,0},{1,0,0}};
    for (int f = 0; f < 6; f++) {
        glm::vec3 n  = N[f];
        glm::vec3 fc = c + n * glm::dot(h, glm::abs(n));
        glm::vec3 hu = U[f] * glm::dot(h, glm::abs(U[f]));
        glm::vec3 hv = Vt[f] * glm::dot(h, glm::abs(Vt[f]));
        glm::vec3 p[4] = {fc - hu - hv, fc + hu - hv, fc + hu + hv, fc - hu + hv};
        unsigned base = (unsigned)(V.size() / 9);
        for (auto& q : p)
            V.insert(V.end(), {q.x, q.y, q.z, n.x, n.y, n.z, col.r, col.g, col.b});
        I.insert(I.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    }
}

// Box recipes per prop type, base at y=0. Colors are fixed per type (props carry their
// own vertex color, so adding a type is just more appendBox calls).
void buildType(PropType t, std::vector<float>& V, std::vector<unsigned>& I) {
    switch (t) {
    case PROP_DUMPSTER:
        appendBox(V, I, {0, 0.55f, 0}, {0.90f, 0.45f, 0.60f}, {0.18f, 0.34f, 0.22f}); // body
        appendBox(V, I, {0, 1.04f, 0}, {0.96f, 0.06f, 0.64f}, {0.10f, 0.16f, 0.12f}); // lid
        break;
    case PROP_CAR:
        appendBox(V, I, {0, 0.45f, 0},   {0.90f, 0.25f, 1.90f}, {0.62f, 0.16f, 0.15f}); // chassis
        appendBox(V, I, {0, 0.86f, -0.1f},{0.72f, 0.22f, 1.00f}, {0.55f, 0.14f, 0.13f}); // cabin
        appendBox(V, I, {0, 0.92f, -0.1f},{0.66f, 0.14f, 0.92f}, {0.12f, 0.16f, 0.20f}); // glass band
        for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            appendBox(V, I, {0.86f * sx, 0.30f, 1.15f * sz},
                      {0.10f, 0.30f, 0.30f}, {0.05f, 0.05f, 0.06f});                     // wheels
        break;
    case PROP_CRATE:
        appendBox(V, I, {0, 0.50f, 0}, {0.50f, 0.50f, 0.50f}, {0.55f, 0.40f, 0.22f});
        break;
    case PROP_LAMP:
        appendBox(V, I, {0, 2.00f, 0},   {0.08f, 2.00f, 0.08f}, {0.22f, 0.22f, 0.24f}); // pole
        appendBox(V, I, {0, 4.05f, 0.12f},{0.24f, 0.12f, 0.30f}, {0.90f, 0.86f, 0.62f}); // head
        break;
    default: break;
    }
}
}  // namespace

bool Props::init() {
    char base[512];
    const char* sdlBase = SDL_GetBasePath();
    snprintf(base, sizeof(base), "%s", sdlBase ? sdlBase : "");
    char vp[600], fp[600];
    snprintf(vp, sizeof(vp), "%sshaders/inst.vert", base);
    snprintf(fp, sizeof(fp), "%sshaders/inst.frag", base);
    if (!shader.load(vp, fp))
        if (!shader.load("shaders/inst.vert", "shaders/inst.frag")) return false;
    snprintf(vp, sizeof(vp), "%sshaders/inst_depth.vert", base);
    snprintf(fp, sizeof(fp), "%sshaders/inst_depth.frag", base);
    if (!depthShader.load(vp, fp))
        if (!depthShader.load("shaders/inst_depth.vert", "shaders/inst_depth.frag")) return false;

    for (int t = 0; t < PROP_COUNT; t++) {
        PropMesh& m = meshes[t];
        std::vector<float>    V;
        std::vector<unsigned> I;
        buildType((PropType)t, V, I);
        m.indexCount = (GLsizei)I.size();

        // Bounding sphere over the built geometry (for per-instance cull at scale 1).
        glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
        for (size_t i = 0; i < V.size(); i += 9) {
            glm::vec3 p(V[i], V[i + 1], V[i + 2]);
            mn = glm::min(mn, p); mx = glm::max(mx, p);
        }
        m.cullCY     = (mn.y + mx.y) * 0.5f;
        m.cullRadius = 0.5f * glm::length(mx - mn);
        // LOD draw distance per type: small clutter vanishes sooner than tall landmarks.
        switch ((PropType)t) {
            case PROP_CRATE:    m.maxDist = 100.0f; break;
            case PROP_DUMPSTER: m.maxDist = 130.0f; break;
            case PROP_CAR:      m.maxDist = 190.0f; break;
            case PROP_LAMP:     m.maxDist = 240.0f; break;
            default:            m.maxDist = 200.0f; break;
        }

        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);
        glGenBuffers(1, &m.ebo);
        glGenBuffers(1, &m.instVBO);
        glBindVertexArray(m.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, V.size() * sizeof(float), V.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, I.size() * sizeof(unsigned), I.data(), GL_STATIC_DRAW);
        const GLsizei vs = 9 * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vs, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vs, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, vs, (void*)(6 * sizeof(float)));
        // Per-instance: pos.xyz (loc 3), yaw+scale (loc 4), divisor 1.
        glBindBuffer(GL_ARRAY_BUFFER, m.instVBO);
        const GLsizei is = 5 * sizeof(float);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, is, (void*)0);
        glVertexAttribDivisor(3, 1);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, is, (void*)(3 * sizeof(float)));
        glVertexAttribDivisor(4, 1);
        glBindVertexArray(0);
    }
    loaded = true;
    return true;
}

void Props::generate() {
    if (!loaded) return;
    for (auto& m : meshes) m.instances.clear();
    PropMesh& dump  = meshes[PROP_DUMPSTER];
    PropMesh& car   = meshes[PROP_CAR];
    PropMesh& crate = meshes[PROP_CRATE];
    PropMesh& lamp  = meshes[PROP_LAMP];

    // Ground height helper: props sit on the terrain (the town pads are flat, so
    // near a building this is just the pad height).
    auto at = [](float x, float z) { return glm::vec3(x, terrainHeight(x, z), z); };

    if (gMapId == MAP_LOBBY) {
        dump.instances.push_back({at(-18.0f, -18.0f), 0.35f, 1.0f});
        car.instances.push_back({at(-30.0f, 18.0f), 1.15f, 1.0f});
        crate.instances.push_back({at(18.0f, -18.0f), 0.4f, 1.0f});
        crate.instances.push_back({at(20.0f, -19.0f), -0.2f, 0.8f});
        lamp.instances.push_back({at(-16.0f, 18.0f), 0.0f, 1.0f});
        lamp.instances.push_back({at(16.0f, 18.0f), 0.0f, 1.0f});
    }

    for (int i = 0; i < gTownBuildingCount; i++) {
        const TownBuilding& b = gTownBuildings[i];
        float hw = b.w * 0.5f, hd = b.d * 0.5f;
        glm::vec3 c = b.center;

        // Lamps at two opposite outer corners.
        lamp.instances.push_back({at(c.x + hw + 1.2f, c.z + hd + 1.2f), 0.0f, 1.0f});
        lamp.instances.push_back({at(c.x - hw - 1.2f, c.z - hd - 1.2f), 0.0f, 1.0f});

        // Dumpster against the +X wall, long side parallel to it.
        float dz = (mapRand(i, 0, 11) * 2 - 1) * hd * 0.5f;
        dump.instances.push_back({at(c.x + hw + 0.9f, c.z + dz), 1.5708f, 1.0f});

        // Crate stack along the -X wall (1..3, varied yaw/scale).
        int nc = 1 + (int)(mapHash(i, 0, 12) % 3);
        for (int k = 0; k < nc; k++) {
            float cz = (mapRand(i, 0, 20 + k) * 2 - 1) * hd * 0.6f;
            crate.instances.push_back({at(c.x - hw - 0.7f, c.z + cz),
                                       mapRand(i, 0, 30 + k) * 6.2831853f,
                                       0.8f + 0.3f * mapRand(i, 0, 40 + k)});
        }

        // A parked car on the seaward yard (toward -X... the beach side), now and then.
        if (mapRand(i, 0, 13) > 0.45f)
            car.instances.push_back({at(c.x + (mapRand(i, 0, 14) * 2 - 1) * hw * 0.6f,
                                        c.z + hd + 3.0f), 0.0f, 1.0f});
    }

    for (auto& m : meshes) {
        m.grid.init(PALDISKI_HALF, 10, -10.0f, 40.0f);
        for (int i = 0; i < (int)m.instances.size(); i++)
            m.grid.insert(m.instances[i].pos.x, m.instances[i].pos.z, i);
        glBindBuffer(GL_ARRAY_BUFFER, m.instVBO);
        glBufferData(GL_ARRAY_BUFFER, m.instances.size() * 5 * sizeof(float),
                     nullptr, GL_DYNAMIC_DRAW);
        m.instCap = (GLint)m.instances.size();
    }
    printf("props: %zu dumpster, %zu car, %zu crate, %zu lamp\n",
           dump.instances.size(), car.instances.size(),
           crate.instances.size(), lamp.instances.size());
}

int Props::cullUpload(PropMesh& m, const Frustum& fr, const glm::vec3& eye) {
    if (m.instances.empty()) return 0;
    packed.clear();
    float maxD2 = m.maxDist * m.maxDist;
    m.grid.forEachVisible(fr, [&](int i) {
        const PropInstance& p = m.instances[i];
        glm::vec3 ctr = p.pos + glm::vec3(0, m.cullCY * p.scale, 0);
        glm::vec3 d = ctr - eye;
        if (glm::dot(d, d) > maxD2) return;                       // LOD distance cull
        if (!fr.sphereVisible(ctr, m.cullRadius * p.scale)) return;
        packed.insert(packed.end(), {p.pos.x, p.pos.y, p.pos.z, p.yaw, p.scale});
    });
    int vis = (int)(packed.size() / 5);
    if (vis == 0) return 0;
    glBindBuffer(GL_ARRAY_BUFFER, m.instVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, packed.size() * sizeof(float), packed.data());
    return vis;
}

void Props::draw(const Renderer& r, const glm::mat4& view, const glm::mat4& proj,
                 const glm::vec3& eye, float time) {
    if (!loaded || empty()) return;
    Frustum fr = Frustum::fromVP(proj * view);
    shader.use();
    shader.setMat4(shader.locView, view);
    shader.setMat4(shader.locProj, proj);
    shader.setVec3(shader.locEye, eye);
    shader.setVec3(shader.locSunDir, r.sunDir);
    shader.setVec3(shader.locSkyZenith, r.skyZenith);
    shader.setVec3(shader.locSkyHorizon, r.skyHorizon);
    shader.setVec3(shader.locGroundAmb, r.groundAmbient);
    shader.setVec3(shader.locSunColor, r.sunColor);
    shader.setFloat(shader.locFogDist, r.fogDist);
    shader.setFloat(shader.locFogHeight, r.fogHeightAmt);
    shader.setFloat(shader.locCloud, r.cloudAmount);
    shader.setFloat(shader.locExposure, r.exposure);
    shader.setFloat(shader.locSaturation, r.saturation);
    shader.setFloat(shader.locTime, time);   // cloud-shadow drift
    shader.setMat4(shader.locLightSpace, r.lightSpace);
    shader.setInt(shader.locShadowMap, 1);
    shader.setInt(shader.locUseShadow, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, r.shadowTex);
    for (auto& m : meshes) {
        int vis = cullUpload(m, fr, eye);
        if (!vis) continue;
        glBindVertexArray(m.vao);
        glDrawElementsInstanced(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, nullptr, vis);
    }
    glBindVertexArray(0);
}

void Props::drawDepth(const glm::mat4& lightSpace, const glm::vec3& focus) {
    if (!loaded || empty()) return;
    Frustum fr = Frustum::fromVP(lightSpace);
    depthShader.use();
    depthShader.setMat4(depthShader.locLightSpace, lightSpace);
    for (auto& m : meshes) {
        int vis = cullUpload(m, fr, focus);
        if (!vis) continue;
        glBindVertexArray(m.vao);
        glDrawElementsInstanced(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, nullptr, vis);
    }
    glBindVertexArray(0);
}

void Props::clear() { for (auto& m : meshes) { m.instances.clear(); m.grid.clear(); } }

bool Props::empty() const {
    for (const auto& m : meshes) if (!m.instances.empty()) return false;
    return true;
}

void Props::destroy() {
    for (auto& m : meshes) {
        if (m.instVBO) glDeleteBuffers(1, &m.instVBO);
        if (m.ebo) glDeleteBuffers(1, &m.ebo);
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
        m.vao = m.vbo = m.ebo = m.instVBO = 0;
        m.instances.clear();
        m.grid.clear();
    }
    shader.destroy();
    depthShader.destroy();
    loaded = false;
}
