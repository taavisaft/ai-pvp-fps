#include "vegetation.h"
#include "renderer.h"
#include "texture.h"
#include "map.h"
#include "tree_scatter.h"
#include "perf.h"
#include <cstdio>

static bool loadPair(Shader& sh, const char* base, const char* v, const char* f) {
    char vp[600], fp[600];
    snprintf(vp, sizeof(vp), "%sshaders/%s", base, v);
    snprintf(fp, sizeof(fp), "%sshaders/%s", base, f);
    if (sh.load(vp, fp)) return true;
    snprintf(vp, sizeof(vp), "shaders/%s", v);
    snprintf(fp, sizeof(fp), "shaders/%s", f);
    return sh.load(vp, fp);
}

void Vegetation::applyQuality(const QualitySettings& q) {
    treeFade0_        = q.treeFade0;
    treeL0End_        = q.treeL0End;
    treeFade1_        = q.treeFade1;
    treeL1End_        = q.treeL1End;
    treeImpFade_      = q.treeImpFade;
    treeImpEnd_       = q.treeImpEnd;
    treeShadowRange_  = q.treeShadowRange;
    bushFade_         = q.bushFade;
    bushEnd_          = q.bushEnd;
    bushShadowRange_  = q.bushShadowRange;
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

bool Vegetation::init(const char* base, GLuint shadow) {
    shadowTex = shadow;
    initMeadowAtlas(base);
    if (!loadPair(vegSh, base, "veg.vert", "veg.frag")) return false;
    if (!loadPair(meadowSh, base, "veg.vert", "meadow.frag")) return false;
    if (!loadPair(impSh, base, "veg_imp.vert", "veg_imp.frag")) return false;
    if (!loadPair(vegDepthSh, base, "veg_depth.vert", "veg_depth.frag")) return false;

    // Needle-spray photo for the branch cards (alpha cutout). Deep mips average
    // the cutout toward transparent and distant crowns thin out — clamp the chain.
    char tp[600];
    snprintf(tp, sizeof(tp), "%stextures/spruce_branch.png", base);
    branchTex = loadTextureRGBA(tp);
    if (!branchTex) branchTex = loadTextureRGBA("textures/spruce_branch.png");
    if (!branchTex) { printf("[veg] missing textures/spruce_branch.png\n"); return false; }
    glBindTexture(GL_TEXTURE_2D, branchTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5);
    snprintf(tp, sizeof(tp), "%stextures/bush_1.png", base);
    bushTex = loadTextureRGBA(tp);
    if (!bushTex) bushTex = loadTextureRGBA("textures/bush_1.png");
    if (!bushTex) { printf("[veg] missing textures/bush_1.png\n"); return false; }
    glBindTexture(GL_TEXTURE_2D, bushTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5);
    glBindTexture(GL_TEXTURE_2D, 0);
    locWind    = glGetUniformLocation(vegSh.program, "windAmp");
    locRange   = glGetUniformLocation(vegSh.program, "grassRange");
    locFadeIn  = glGetUniformLocation(vegSh.program, "fadeIn");
    locFadeOut = glGetUniformLocation(vegSh.program, "fadeOut");
    locBake    = glGetUniformLocation(vegSh.program, "bake");
    locWindD   = glGetUniformLocation(vegDepthSh.program, "windAmp");
    locMeadowEye = glGetUniformLocation(vegDepthSh.program,"grassEye");
    locMeadowRange = glGetUniformLocation(vegDepthSh.program,"grassRange");
    locCoverage    = glGetUniformLocation(vegSh.program, "alphaToCoverage");
    locImpCoverage = glGetUniformLocation(impSh.program, "alphaToCoverage");
    locImpSize    = glGetUniformLocation(impSh.program, "impSize");
    locImpFadeIn  = glGetUniformLocation(impSh.program, "fadeIn");
    locImpFadeOut = glGetUniformLocation(impSh.program, "fadeOut");
    impSh.use();
    glUniform1i(glGetUniformLocation(impSh.program, "landscapeMap"), 8);
    glUniform1i(glGetUniformLocation(impSh.program, "impTex"), 5);   // atlas unit
    vegSh.use();
    glUniform1i(glGetUniformLocation(vegSh.program, "branchTex"), 6);
    glUniform1i(glGetUniformLocation(vegSh.program, "shadowMap"), 1); // shadow depth unit
    meadowSh.use();
    glUniform1i(glGetUniformLocation(meadowSh.program, "branchTex"), 6);
    glUniform1i(glGetUniformLocation(meadowSh.program, "shadowMap"), 1);
    locGrassWind  = glGetUniformLocation(meadowSh.program, "windAmp");
    locGrassRange = glGetUniformLocation(meadowSh.program, "grassRange");
    vegDepthSh.use();
    glUniform1i(glGetUniformLocation(vegDepthSh.program, "branchTex"), 6);

    std::vector<float> v;
    std::vector<unsigned> idx;
    vegBuildSpruce(v, idx, /*low=*/false);
    uploadMesh(l0Vbo, l0Ebo, l0Idx, v, idx);
    v.clear(); idx.clear();
    vegBuildSpruce(v, idx, /*low=*/true);
    uploadMesh(l1Vbo, l1Ebo, l1Idx, v, idx);
    v.clear(); idx.clear();
    vegBuildBush(v, idx);
    uploadMesh(bushVbo, bushEbo, bushIdx, v, idx);

    glGenBuffers(1, &streamL0);
    glGenBuffers(1, &streamL1);
    glGenBuffers(1, &streamImp);
    glGenBuffers(1, &streamShadow);
    glGenBuffers(1, &streamBush);
    glGenBuffers(1, &streamBushShadow);
    vaoL0     = vegMakeVAO(l0Vbo, l0Ebo, streamL0);
    vaoL1     = vegMakeVAO(l1Vbo, l1Ebo, streamL1);
    vaoShadow = vegMakeVAO(l0Vbo, l0Ebo, streamShadow);
    vaoBush       = vegMakeVAO(bushVbo, bushEbo, streamBush);
    vaoBushShadow = vegMakeVAO(bushVbo, bushEbo, streamBushShadow);

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

    return initTrainingTrees(base) && vegBakeImpostor(*this, 256, 512);   // caller restores the viewport
}

// Deterministic spruce scatter: one candidate per ~7 m cell, kept by the pine
// biome density (plus rare lone stragglers), rejected on water/sand, steep
// slopes and alpine height. Same math every launch — no sync, no assets.
void Vegetation::buildTrees() {
    placed = true;
    trees.clear();
    // Placement now lives in tree_scatter.cpp (shared with the server, so collision
    // matches). setMap() fills gTrees; consume it here for the rendered instances.
    if (gTrees.empty()) buildTreeColliders();   // safety if drawn before a setMap
    trees.reserve(gTrees.size());
    bufL0.reserve(gTrees.size()*8); bufL1.reserve(gTrees.size()*8);
    bufImp.reserve(gTrees.size()*8); bufShadow.reserve(gTrees.size()*8);
    const bool training = gMapId == MAP_LOBBY;
    for (const TreeInstance& g : gTrees)
        trees.push_back({{g.x, g.y, g.z}, g.scale, g.yaw, g.tint,
                         training ? (uint8_t)trainingTreeType(g.x, g.z) : (uint8_t)0});
    if (training) reserveTrainingStaging();

    float half  = (gMapId == MAP_LOBBY) ? LOBBY_HALF : PALDISKI_HALF;
    int   cells = (gMapId == MAP_LOBBY) ? 64 : 32;
    float yMax  = 110.0f;
    for (const Tree& t : trees) yMax = fmaxf(yMax, t.pos.y + t.scale * 1.1f + 1.0f);
    grid.init(half, cells, 0.0f, yMax);
    for (int i = 0; i < (int)trees.size(); i++)
        grid.insert(trees[i].pos.x, trees[i].pos.z, i);
    printf("[veg] %d trees (shared scatter)\n", (int)trees.size());
    if(gMapId==MAP_LOBBY) logTrainingTreeMix();
}

// Deterministic bush scatter, forest-edge biased: b*(1-b) peaks where the pine
// biome thins out (stand edges, clearings) — that's where real undergrowth gets
// light. A small flat term dots the open meadows. Same no-sync/no-asset rules as
// the tree scatter.
void Vegetation::buildBushes() {
    bushes.clear();
    if (gMapId == MAP_LOBBY) {
        // Training keeps the meadow and trees without standalone berry bushes.
        // Keep a valid empty grid for the lit and shadow visibility traversals.
        bushGrid.init(LOBBY_HALF, 16, 0.0f, 2.0f);
        return;
    }
    const float STEP = 7.0f;
    const int n = (int)(2.0f * PALDISKI_HALF / STEP);
    for (int iz = 0; iz < n; iz++)
        for (int ix = 0; ix < n; ix++) {
            float x = -PALDISKI_HALF + (ix + 0.5f) * STEP + (mapRand(ix, iz, 51) - 0.5f) * 6.0f;
            float z = -PALDISKI_HALF + (iz + 0.5f) * STEP + (mapRand(ix, iz, 52) - 0.5f) * 6.0f;
            if (forestSiteClearance(x, z, 1.2f)) continue;
            float b = pineForestBiome(x, z);
            float dens = b * (1.0f - b) * 2.4f + 0.028f;
            if (mapRand(ix, iz, 53) > dens) continue;
            float h = terrainHeight(x, z);
            if (h < 1.6f || h > 95.0f) continue;
            float gx = (terrainHeight(x + 2.0f, z) - terrainHeight(x - 2.0f, z)) * 0.25f;
            float gz = (terrainHeight(x, z + 2.0f) - terrainHeight(x, z - 2.0f)) * 0.25f;
            if (gx * gx + gz * gz > 0.30f) continue;
            Tree t;
            t.pos   = {x, h - 0.04f, z};
            t.scale = 0.5f + mapRand(ix, iz, 54) * 0.7f;   // 0.5-1.2 m tall
            t.yaw   = mapRand(ix, iz, 55) * 6.2831853f;
            t.tint  = 0.85f + mapRand(ix, iz, 56) * 0.35f;
            bushes.push_back(t);
        }
    bushGrid.init(PALDISKI_HALF, 32, 0.0f, 3.0f);
    for (int i = 0; i < (int)bushes.size(); i++)
        bushGrid.insert(bushes[i].pos.x, bushes[i].pos.z, i);
    printf("[veg] scattered %d bushes\n", (int)bushes.size());
}

void Vegetation::invalidate() {
    placed = false;
    landscapeSun = glm::vec3(0);
    trees.clear();
    grid.clear();
    bushes.clear();
    bushGrid.clear();
}

void Vegetation::destroy() {
    destroyMeadow();
    destroyLandscape();
    destroyTrainingTrees();
    GLuint vaos[] = {vaoL0, vaoL1, vaoImp, vaoShadow, vaoBush, vaoBushShadow};
    for (GLuint v : vaos) if (v) glDeleteVertexArrays(1, &v);
    GLuint bufs[] = {l0Vbo, l0Ebo, l1Vbo, l1Ebo, quadVbo,
                     bushVbo, bushEbo,
                     streamL0, streamL1, streamImp, streamShadow,
                     streamBush, streamBushShadow};
    for (GLuint b : bufs) if (b) glDeleteBuffers(1, &b);
    vaoL0 = vaoL1 = vaoImp = vaoShadow = vaoBush = vaoBushShadow = 0;
    l0Vbo = l0Ebo = l1Vbo = l1Ebo = quadVbo = 0;
    bushVbo = bushEbo = 0;
    streamL0 = streamL1 = streamImp = streamShadow = 0;
    streamBush = streamBushShadow = 0;
    if (impTex) glDeleteTextures(1, &impTex);
    impTex = 0;
    if (branchTex) glDeleteTextures(1, &branchTex);
    branchTex = 0;
    if (bushTex) glDeleteTextures(1, &bushTex);
    bushTex = 0;
    vegSh.destroy();
    meadowSh.destroy();
    impSh.destroy();
    vegDepthSh.destroy();
    trees.clear();
    grid.clear();
    bushes.clear();
    bushGrid.clear();
    placed = false;
}
