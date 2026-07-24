#include "vegetation.h"
#include "renderer.h"
#include "map.h"
#include <cstdio>

// GLSL smoothstep twin (terrSmooth(terrClamp01()) is the same cubic).
static float sstep(float e0, float e1, float x) {
    return terrSmooth(terrClamp01((x - e0) / (e1 - e0)));
}

static bool loadPair(Shader& sh, const char* base, const char* v, const char* f) {
    char vp[600], fp[600];
    snprintf(vp, sizeof(vp), "%sshaders/%s", base, v);
    snprintf(fp, sizeof(fp), "%sshaders/%s", base, f);
    if (sh.load(vp, fp)) return true;
    snprintf(vp, sizeof(vp), "shaders/%s", v);
    snprintf(fp, sizeof(fp), "shaders/%s", f);
    return sh.load(vp, fp);
}

static void uploadMesh(GLuint& vbo, GLuint& ebo, GLsizei& count,
                       const std::vector<float>& v, const std::vector<unsigned>& idx) {
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(),
                 GL_STATIC_DRAW);
    count = (GLsizei)idx.size();
}

static void pushTree(std::vector<float>& b, const Vegetation::Tree& t) {
    b.insert(b.end(), {t.pos.x, t.pos.y, t.pos.z, t.scale,
                       t.yaw, t.yaw * 0.159f, t.tint, 0.0f});
}

static void drawStream(GLuint vao, GLuint stream, GLsizei idxCount,
                       const std::vector<float>& buf) {
    if (buf.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, stream);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(),
                 GL_STREAM_DRAW);
    glBindVertexArray(vao);
    glDrawElementsInstanced(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, nullptr,
                            (GLsizei)(buf.size() / 8));
    glBindVertexArray(0);
}

bool Vegetation::init(const char* base) {
    if (!loadPair(vegSh, base, "veg.vert", "veg.frag")) return false;
    if (!loadPair(impSh, base, "veg_imp.vert", "veg_imp.frag")) return false;
    if (!loadPair(vegDepthSh, base, "veg_depth.vert", "depth.frag")) return false;
    locWind    = glGetUniformLocation(vegSh.program, "windAmp");
    locRange   = glGetUniformLocation(vegSh.program, "grassRange");
    locFadeIn  = glGetUniformLocation(vegSh.program, "fadeIn");
    locFadeOut = glGetUniformLocation(vegSh.program, "fadeOut");
    locBake    = glGetUniformLocation(vegSh.program, "bake");
    locWindD   = glGetUniformLocation(vegDepthSh.program, "windAmp");
    locImpSize   = glGetUniformLocation(impSh.program, "impSize");
    locImpFadeIn = glGetUniformLocation(impSh.program, "fadeIn");
    impSh.use();
    glUniform1i(glGetUniformLocation(impSh.program, "impTex"), 5);   // atlas unit

    std::vector<float> v;
    std::vector<unsigned> idx;
    vegBuildBlade(v, idx);
    uploadMesh(bladeVbo, bladeEbo, bladeIdx, v, idx);
    v.clear(); idx.clear();
    vegBuildSpruce(v, idx, /*low=*/false);
    uploadMesh(l0Vbo, l0Ebo, l0Idx, v, idx);
    v.clear(); idx.clear();
    vegBuildSpruce(v, idx, /*low=*/true);
    uploadMesh(l1Vbo, l1Ebo, l1Idx, v, idx);

    glGenBuffers(1, &streamL0);
    glGenBuffers(1, &streamL1);
    glGenBuffers(1, &streamImp);
    glGenBuffers(1, &streamShadow);
    vaoL0     = vegMakeVAO(l0Vbo, l0Ebo, streamL0);
    vaoL1     = vegMakeVAO(l1Vbo, l1Ebo, streamL1);
    vaoShadow = vegMakeVAO(l0Vbo, l0Ebo, streamShadow);

    // Impostor quad: corner.xy + uv, drawn as a 4-vertex triangle strip.
    const float quad[16] = {-0.5f, 0, 0, 0,  0.5f, 0, 1, 0,
                            -0.5f, 1, 0, 1,  0.5f, 1, 1, 1};
    glGenBuffers(1, &quadVbo);
    glGenVertexArrays(1, &vaoImp);
    glBindVertexArray(vaoImp);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindBuffer(GL_ARRAY_BUFFER, streamImp);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
    glVertexAttribDivisor(5, 1);
    glBindVertexArray(0);

    return vegBakeImpostor(*this, 384, 512);   // caller restores the viewport
}

// Deterministic spruce scatter: one candidate per ~7 m cell, kept by the pine
// biome density (plus rare lone stragglers), rejected on water/sand, steep
// slopes and alpine height. Same math every launch — no sync, no assets.
void Vegetation::buildTrees() {
    placed = true;
    trees.clear();
    if (gMapId == MAP_LOBBY) {
        // Lobby counterpart of the taiga forest (every feature ships here in
        // miniature): a loose spruce ring outside the pad, firing lane kept clear.
        for (int i = 0; i < 40; i++) {
            float a  = (i / 40.0f + (mapRand(i, 1, 61) - 0.5f) * 0.02f) * 6.2831853f;
            float rr = 34.0f + mapRand(i, 2, 62) * 22.0f;
            float x = cosf(a) * rr, z = sinf(a) * rr;
            if (x > 8.0f && fabsf(z) < 16.0f) continue;   // range lane to the wall
            Tree t;
            t.pos   = {x, terrainHeight(x, z) - 0.15f, z};
            t.scale = mapRand(i, 3, 63) < 0.4f ? 2.5f + mapRand(i, 4, 64) * 2.0f
                                               : 6.5f + mapRand(i, 4, 64) * 4.0f;
            t.yaw   = mapRand(i, 5, 65) * 6.2831853f;
            t.tint  = 0.82f + mapRand(i, 6, 66) * 0.36f;
            trees.push_back(t);
        }
        grid.init(LOBBY_HALF, 16, 0.0f, 40.0f);
        for (int i = 0; i < (int)trees.size(); i++)
            grid.insert(trees[i].pos.x, trees[i].pos.z, i);
        return;
    }
    const float STEP = 5.0f;
    const int n = (int)(2.0f * PALDISKI_HALF / STEP);
    for (int iz = 0; iz < n; iz++)
        for (int ix = 0; ix < n; ix++) {
            float x = -PALDISKI_HALF + (ix + 0.5f) * STEP + (mapRand(ix, iz, 41) - 0.5f) * 4.2f;
            float z = -PALDISKI_HALF + (iz + 0.5f) * STEP + (mapRand(ix, iz, 42) - 0.5f) * 4.2f;
            // Squared term packs the patch cores tight (~5 m spacing) while the
            // linear tail keeps thin stands and lone stragglers on the meadows.
            float b = pineForestBiome(x, z);
            float dens = b * b * 2.2f + b * 0.30f + 0.045f;
            if (mapRand(ix, iz, 43) > dens) continue;
            float h = terrainHeight(x, z);
            if (h < 1.6f || h > 95.0f) continue;
            float gx = (terrainHeight(x + 2.0f, z) - terrainHeight(x - 2.0f, z)) * 0.25f;
            float gz = (terrainHeight(x, z + 2.0f) - terrainHeight(x, z - 2.0f)) * 0.25f;
            if (gx * gx + gz * gz > 0.30f) continue;
            Tree t;
            t.pos = {x, h - 0.15f, z};
            // Stand cores are mature forest; edges and open bog skew young — the
            // reference meadows are dotted with 2-4 m saplings, not full spruces.
            float r     = mapRand(ix, iz, 44);
            float young = 2.2f + r * 2.6f;               // 2.2-4.8 m sapling
            float grown = 6.5f + r * 5.0f;               // 6.5-11.5 m stand tree
            float pYoung = b < 0.35f ? 0.78f : 0.18f;    // meadow vs core mix
            t.scale = mapRand(ix, iz, 47) < pYoung ? young : grown;
            t.yaw   = mapRand(ix, iz, 45) * 6.2831853f;
            t.tint  = 0.82f + mapRand(ix, iz, 46) * 0.36f;
            trees.push_back(t);
        }
    grid.init(PALDISKI_HALF, 32, 0.0f, 110.0f);
    for (int i = 0; i < (int)trees.size(); i++)
        grid.insert(trees[i].pos.x, trees[i].pos.z, i);
    printf("[veg] scattered %d trees\n", (int)trees.size());
}

// Fill one 16 m grass tile: jittered candidates masked by the same dirt-field /
// forest-floor / sand / slope rules the ground shader colors by, so blades stand
// exactly where the ground reads grassy. Build-time only work.
void Vegetation::rebuildTile(GrassTile& t, int tx, int tz) {
    bufTile.clear();
    const float x0 = tx * GRASS_TILE, z0 = tz * GRASS_TILE;
    const int candidates = (int)(GRASS_TILE * GRASS_TILE * GRASS_PER_M2);
    t.minY = 1e9f; t.maxY = -1e9f;
    for (int i = 0; i < candidates; i++) {
        int cx = tx * 4096 + i;   // unique hash coords per candidate
        float rx = x0 + mapRand(cx, tz, 71) * GRASS_TILE;
        float rz = z0 + mapRand(cx, tz, 72) * GRASS_TILE;
        if (fabsf(rx) > PALDISKI_HALF || fabsf(rz) > PALDISKI_HALF) continue;
        float h = terrainHeight(rx, rz);
        if (h < 1.25f || h > 95.0f) continue;            // sand/water & alpine rock
        float sx = terrainHeight(rx + 1.2f, rz) - h;
        float sz = terrainHeight(rx, rz + 1.2f) - h;
        if (sx * sx + sz * sz > 0.55f) continue;         // steep = rock face
        float region = vegFbm(rx * 0.004f, rz * 0.004f); // dirt fields: sparse
        float keep = (1.0f - sstep(0.42f, 0.60f, region) * 0.9f)
                   * (1.0f - 0.65f * pineForestBiome(rx, rz));
        if (mapRand(cx, tz, 73) > keep) continue;
        float dry = sstep(0.72f, 0.88f, vegFbm(rx * 0.4f + 10.0f, rz * 0.4f + 10.0f));
        bufTile.insert(bufTile.end(),
            {rx, h - 0.02f, rz, 0.55f + mapRand(cx, tz, 74) * 0.55f,
             mapRand(cx, tz, 75) * 6.2831853f, mapRand(cx, tz, 76),
             0.80f + mapRand(cx, tz, 77) * 0.45f, dry * 0.55f});
        if (h < t.minY) t.minY = h;
        if (h > t.maxY) t.maxY = h;
    }
    if (t.minY > t.maxY) { t.minY = 0.0f; t.maxY = 0.0f; }
    if (!t.vbo) glGenBuffers(1, &t.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, t.vbo);
    glBufferData(GL_ARRAY_BUFFER, bufTile.size() * sizeof(float), bufTile.data(),
                 GL_STATIC_DRAW);
    if (!t.vao) t.vao = vegMakeVAO(bladeVbo, bladeEbo, t.vbo);
    t.count = (int)(bufTile.size() / 8);
    t.tx = tx; t.tz = tz;
}

void Vegetation::drawLit(const Renderer& r, const Frustum& fr, const glm::vec3& eye) {
    if (!placed) buildTrees();

    // Bucket visible trees by distance. Buckets OVERLAP across the fade bands —
    // both LODs of a transitioning tree draw, split per-pixel by the dither.
    bufL0.clear(); bufL1.clear(); bufImp.clear();
    grid.forEachVisible(fr, [&](int i) {
        const Tree& t = trees[i];
        glm::vec3 d = t.pos - eye;
        float dist = sqrtf(glm::dot(d, d));
        if (dist < TREE_L0_END) pushTree(bufL0, t);
        if (dist > TREE_FADE0 && dist < TREE_L1_END) pushTree(bufL1, t);
        if (dist > TREE_FADE1) pushTree(bufImp, t);
    });

    vegSh.use();
    vegSh.setMat4(vegSh.locView, r.curView);
    vegSh.setMat4(vegSh.locProj, r.curProj);
    vegSh.setVec3(vegSh.locEye, eye);
    vegSh.setFloat(vegSh.locTime, r.frameTime);
    vegSh.setMat4(vegSh.locLightSpace, r.lightSpace);
    vegSh.setInt(vegSh.locShadowMap, 1);
    vegSh.setInt(vegSh.locUseShadow, 1);
    vegSh.setInt(locBake, 0);
    vegSh.setVec3(vegSh.locSunDir, r.sunDir);
    vegSh.setVec3(vegSh.locSunColor, r.sunColor);
    vegSh.setVec3(vegSh.locSkyZenith, r.skyZenith);
    vegSh.setVec3(vegSh.locSkyHorizon, r.skyHorizon);
    vegSh.setVec3(vegSh.locGroundAmb, r.groundAmbient);
    vegSh.setFloat(vegSh.locFogDist, r.fogDist);
    vegSh.setFloat(vegSh.locFogHeight, r.fogHeightAmt);
    vegSh.setFloat(vegSh.locCloud, r.cloudAmount);
    vegSh.setFloat(vegSh.locExposure, r.exposure);
    vegSh.setFloat(vegSh.locSaturation, r.saturation);

    // Tree LOD0: full mesh, dithers out across the first band.
    vegSh.setFloat(locWind, 0.05f);
    vegSh.setFloat(locRange, 0.0f);
    glUniform2f(locFadeIn, 0.0f, 0.0f);
    glUniform2f(locFadeOut, TREE_FADE0, TREE_L0_END);
    drawStream(vaoL0, streamL0, l0Idx, bufL0);
    // Tree LOD1: dithers in against LOD0, out against the impostors.
    glUniform2f(locFadeIn, TREE_FADE0, TREE_L0_END);
    glUniform2f(locFadeOut, TREE_FADE1, TREE_L1_END);
    drawStream(vaoL1, streamL1, l1Idx, bufL1);

    // Grass: camera-centered tile ring; stale slots rebuilt within a budget (a
    // fresh slot's blades are still height-zero at the range edge, so a one-frame
    // delay is invisible).
    if (GRASS_ENABLED) {
    vegSh.setFloat(locWind, 0.045f);
    vegSh.setFloat(locRange, GRASS_RANGE);
    glUniform2f(locFadeIn, 0.0f, 0.0f);
    glUniform2f(locFadeOut, 0.0f, 0.0f);
    const int S = GRASS_SLOTS;
    int ctx = (int)floorf(eye.x / GRASS_TILE);
    int ctz = (int)floorf(eye.z / GRASS_TILE);
    // Rebuild budget: a tile build costs ~1k terrain samples, so keep the steady-
    // state trickle small; only a fresh map (everything stale) gets a big burst.
    int stale = 0;
    for (auto& row : tiles) for (auto& t : row) if (t.tx == INT_MIN) stale++;
    int budget = stale > 120 ? 60 : 3;
    for (int dz = -GRASS_RING; dz <= GRASS_RING; dz++)
        for (int dx = -GRASS_RING; dx <= GRASS_RING; dx++) {
            int tx = ctx + dx, tz = ctz + dz;
            float x0 = tx * GRASS_TILE, z0 = tz * GRASS_TILE;
            float nx = fmaxf(fabsf(eye.x - (x0 + GRASS_TILE * 0.5f)) - GRASS_TILE * 0.5f, 0.0f);
            float nz = fmaxf(fabsf(eye.z - (z0 + GRASS_TILE * 0.5f)) - GRASS_TILE * 0.5f, 0.0f);
            if (nx * nx + nz * nz > GRASS_RANGE * GRASS_RANGE) continue;
            GrassTile& t = tiles[(tx % S + S) % S][(tz % S + S) % S];
            if (t.tx != tx || t.tz != tz) {
                if (budget <= 0) continue;
                budget--;
                rebuildTile(t, tx, tz);
            }
            if (!t.count) continue;
            glm::vec3 c(x0 + GRASS_TILE * 0.5f, (t.minY + t.maxY) * 0.5f,
                        z0 + GRASS_TILE * 0.5f);
            glm::vec3 half(GRASS_TILE * 0.5f, (t.maxY - t.minY) * 0.5f + 0.9f,
                           GRASS_TILE * 0.5f);
            if (!fr.aabbVisible(c, half)) continue;
            glBindVertexArray(t.vao);
            glDrawElementsInstanced(GL_TRIANGLES, bladeIdx, GL_UNSIGNED_INT, nullptr,
                                    t.count);
        }
    }
    glBindVertexArray(0);

    // Far trees: baked billboard per tree, out to the map edge — every tree is
    // always drawn somewhere, nothing appears out of thin air.
    if (!bufImp.empty()) {
        impSh.use();
        impSh.setMat4(impSh.locView, r.curView);
        impSh.setMat4(impSh.locProj, r.curProj);
        impSh.setVec3(impSh.locEye, eye);
        impSh.setFloat(impSh.locTime, r.frameTime);
        impSh.setVec3(impSh.locSunColor, r.sunColor);
        impSh.setVec3(impSh.locSkyZenith, r.skyZenith);
        impSh.setVec3(impSh.locSkyHorizon, r.skyHorizon);
        impSh.setVec3(impSh.locGroundAmb, r.groundAmbient);
        impSh.setFloat(impSh.locFogDist, r.fogDist);
        impSh.setFloat(impSh.locFogHeight, r.fogHeightAmt);
        impSh.setFloat(impSh.locCloud, r.cloudAmount);
        impSh.setFloat(impSh.locExposure, r.exposure);
        impSh.setFloat(impSh.locSaturation, r.saturation);
        glUniform2f(locImpSize, impSize.x, impSize.y);
        glUniform2f(locImpFadeIn, TREE_FADE1, TREE_L1_END);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, impTex);
        glActiveTexture(GL_TEXTURE0);
        glBindBuffer(GL_ARRAY_BUFFER, streamImp);
        glBufferData(GL_ARRAY_BUFFER, bufImp.size() * sizeof(float), bufImp.data(),
                     GL_STREAM_DRAW);
        glBindVertexArray(vaoImp);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(bufImp.size() / 8));
        glBindVertexArray(0);
    }
}

void Vegetation::drawShadow(const Frustum& sunFr, const glm::vec3& focus, float time,
                            const glm::mat4& lightSpace) {
    if (!placed) buildTrees();
    bufShadow.clear();
    grid.forEachVisible(sunFr, [&](int i) {
        const Tree& t = trees[i];
        glm::vec3 d = t.pos - focus;
        if (glm::dot(d, d) < TREE_SHADOW_RANGE * TREE_SHADOW_RANGE)
            pushTree(bufShadow, t);
    });
    if (bufShadow.empty()) return;
    vegDepthSh.use();
    vegDepthSh.setMat4(vegDepthSh.locLightSpace, lightSpace);
    vegDepthSh.setFloat(vegDepthSh.locTime, time);
    glUniform1f(locWindD, 0.05f);
    drawStream(vaoShadow, streamShadow, l0Idx, bufShadow);
}

void Vegetation::invalidate() {
    placed = false;
    trees.clear();
    grid.clear();
    for (auto& row : tiles)
        for (auto& t : row) { t.tx = INT_MIN; t.tz = INT_MIN; t.count = 0; }
}

void Vegetation::destroy() {
    for (auto& row : tiles)
        for (auto& t : row) {
            if (t.vao) glDeleteVertexArrays(1, &t.vao);
            if (t.vbo) glDeleteBuffers(1, &t.vbo);
            t = GrassTile{};
        }
    GLuint vaos[] = {vaoL0, vaoL1, vaoImp, vaoShadow};
    for (GLuint v : vaos) if (v) glDeleteVertexArrays(1, &v);
    GLuint bufs[] = {bladeVbo, bladeEbo, l0Vbo, l0Ebo, l1Vbo, l1Ebo, quadVbo,
                     streamL0, streamL1, streamImp, streamShadow};
    for (GLuint b : bufs) if (b) glDeleteBuffers(1, &b);
    vaoL0 = vaoL1 = vaoImp = vaoShadow = 0;
    bladeVbo = bladeEbo = l0Vbo = l0Ebo = l1Vbo = l1Ebo = quadVbo = 0;
    streamL0 = streamL1 = streamImp = streamShadow = 0;
    if (impTex) glDeleteTextures(1, &impTex);
    impTex = 0;
    vegSh.destroy();
    impSh.destroy();
    vegDepthSh.destroy();
    trees.clear();
    grid.clear();
    placed = false;
}
