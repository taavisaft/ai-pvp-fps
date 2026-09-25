#include "vegetation.h"
#include "renderer.h"
#include "map.h"
#include "perf.h"
static void pushTree(std::vector<float>& b, const Vegetation::Tree& t) {
    b.insert(b.end(), {t.pos.x, t.pos.y, t.pos.z, t.scale,
                       t.yaw, t.yaw * 0.159f, t.tint, t.interior});
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

void Vegetation::drawLit(const Renderer& r, const Frustum& fr, const glm::vec3& eye) {
    if (!placed) { buildTrees(); buildBushes(); }

    if (landscapeSun != r.sunDir) bakeLandscape(r.sunDir);
    const float fade1=treeFade1_;
    const float end1=treeL1End_;
    gVegStats.reset();
    // Bucket visible trees by distance. Buckets OVERLAP across the fade bands —
    // both LODs of a transitioning tree draw, split per-pixel by the dither.
    bufBush.clear();
    for (SpeciesStaging* staging : {&speciesL0, &speciesL1, &speciesImp, &speciesShrub})
        for (auto& buf : *staging) buf.clear();
    const float reach = r.reflectionPass ? 420.0f : treeImpEnd_;
    grid.forEachVisibleWithin(fr, eye, reach, [&](int i) {
        const Tree& t = trees[i];
        glm::vec3 d = t.pos - eye;
        float dist = sqrtf(glm::dot(d, d));
        const bool shrub = t.scale < SHRUB_SCALE;
        if (shrub && dist < end1) { pushTree(speciesShrub[t.type], t); gVegStats.treesL1++; }
        if (!shrub && dist < treeL0End_ && !r.reflectionPass) { pushTree(speciesL0[t.type], t); gVegStats.treesL0++; }
        if (!shrub && (dist > treeFade0_ || r.reflectionPass) && dist < end1) { pushTree(speciesL1[t.type], t); gVegStats.treesL1++; }
        if (dist > fade1 && dist < reach) { pushTree(speciesImp[t.type], t); gVegStats.treesImp++; }
    });
    bushGrid.forEachVisible(fr, [&](int i) {
        const Tree& t = bushes[i];
        glm::vec3 d = t.pos - eye;
        if (glm::dot(d, d) < bushEnd_ * bushEnd_) { pushTree(bufBush, t); gVegStats.bushes++; }
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
    vegSh.setFloat(vegSh.locClipWater,r.reflectionPass ? TRAINING_POND_Y : 0);
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
    vegSh.setFloat(vegSh.locHazeCool, r.hazeCool);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, trainingBranchTex);
    vegSh.setInt(locTrainingTree,1);
    glActiveTexture(GL_TEXTURE0);

    // Tree LOD0: full mesh, dithers out across the first band.
    vegSh.setFloat(locWind, 0.05f);
    vegSh.setFloat(locRange, 0.0f);
    glUniform2f(locFadeIn, 0.0f, 0.0f);
    glUniform2f(locFadeOut, treeFade0_, treeL0End_);
    drawTrainingTreeStream(speciesL0,TRAINING_L0);
    // Tree LOD1: dithers in against LOD0, out against the impostors.
    glUniform2f(locFadeIn, 0.0f, 0.0f);
    glUniform2f(locFadeOut, fade1, end1);
    drawTrainingTreeStream(speciesShrub,TRAINING_L1);
    if (r.reflectionPass) glUniform2f(locFadeIn, 0.0f, 0.0f);
    else glUniform2f(locFadeIn, treeFade0_, treeL0End_);
    glUniform2f(locFadeOut, fade1, end1);
    drawTrainingTreeStream(speciesL1,TRAINING_L1);

    vegSh.setInt(locTrainingTree,0);
    // Bushes: same shader, own photo on the shared sampler unit; dither fully
    // out by bushEnd_ (nothing fades in behind them — undergrowth just ends).
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, bushTex);
    glActiveTexture(GL_TEXTURE0);
    vegSh.setFloat(locWind, 0.06f);
    glUniform2f(locFadeIn, 0.0f, 0.0f);
    glUniform2f(locFadeOut, bushFade_, bushEnd_);
    drawStream(vaoBush, streamBush, bushIdx, bufBush);

    if(!r.reflectionPass) drawMeadow(r,fr,eye);
    glBindVertexArray(0);

    // Far trees: baked billboard per tree, out to the map edge — every tree is
    // always drawn somewhere, nothing appears out of thin air.
    {
        impSh.use();
        impSh.setFloat(impSh.locClipWater,r.reflectionPass ? TRAINING_POND_Y : 0);
        impSh.setMat4(impSh.locView, r.curView);
        impSh.setMat4(impSh.locProj, r.curProj);
        impSh.setVec3(impSh.locEye, eye);
        impSh.setFloat(impSh.locTime, r.frameTime);
        impSh.setVec3(impSh.locSunDir, r.sunDir);
        impSh.setVec3(impSh.locSunColor, r.sunColor);
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, landscapeTex);
        glActiveTexture(GL_TEXTURE0);
        impSh.setVec3(impSh.locSkyZenith, r.skyZenith);
        impSh.setVec3(impSh.locSkyHorizon, r.skyHorizon);
        impSh.setVec3(impSh.locGroundAmb, r.groundAmbient);
        impSh.setFloat(impSh.locFogDist, r.fogDist);
        impSh.setFloat(impSh.locFogHeight, r.fogHeightAmt);
        impSh.setFloat(impSh.locCloud, r.cloudAmount);
        impSh.setFloat(impSh.locExposure, r.exposure);
        impSh.setFloat(impSh.locSaturation, r.saturation);
        impSh.setFloat(impSh.locHazeCool, r.hazeCool);
                glUniform2f(locImpFadeIn, fade1, end1);
        glUniform2f(locImpFadeOut, treeImpFade_, treeImpEnd_);
        drawTrainingTreeStream(speciesImp,TRAINING_IMPOSTOR);
    }
}

void Vegetation::drawShadow(const Frustum& sunFr, const glm::vec3& focus, float time,
                            const glm::mat4& lightSpace) {
    if (!placed) { buildTrees(); buildBushes(); }

    bufBushShadow.clear();
    for (SpeciesStaging* staging : {&speciesShadow, &speciesShadowLow})
        for (auto& buf : *staging) buf.clear();
    const float shadowDetail = treeFade0_ * .4f;
    bool anyTree = false;
    grid.forEachVisible(sunFr, [&](int i) {
        const Tree& t = trees[i];
        glm::vec3 d = t.pos - focus;
        float distanceSq = glm::dot(d, d);
        pushTree(distanceSq < shadowDetail * shadowDetail && t.scale >= SHRUB_SCALE ? speciesShadow[t.type] : speciesShadowLow[t.type], t);
        anyTree = true;
    });
    bushGrid.forEachVisible(sunFr, [&](int i) {
        const Tree& t = bushes[i];
        glm::vec3 d = t.pos - focus;
        if (glm::dot(d, d) < bushShadowRange_ * bushShadowRange_)
            pushTree(bufBushShadow, t);
    });
    if (!anyTree && bufBushShadow.empty()) return;
    vegDepthSh.use();
    vegDepthSh.setMat4(vegDepthSh.locLightSpace, lightSpace);
    vegDepthSh.setFloat(vegDepthSh.locTime, time);
    glUniform1f(locWindD, 0.05f);
    vegDepthSh.setFloat(locMeadowRange,0);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, trainingBranchTex);
    vegDepthSh.setInt(locTrainingTreeD,1);
    drawTrainingTreeStream(speciesShadow,TRAINING_SHADOW);
    drawTrainingTreeStream(speciesShadowLow,TRAINING_SHADOW_LOW);
    vegDepthSh.setInt(locTrainingTreeD,0);
    glBindTexture(GL_TEXTURE_2D, bushTex);
    glActiveTexture(GL_TEXTURE0);
    drawStream(vaoBushShadow, streamBushShadow, bushIdx, bufBushShadow);
    // Grass receives world shadows in the lit pass, but does not cast them.
}
