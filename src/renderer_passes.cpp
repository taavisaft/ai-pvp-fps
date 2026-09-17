#include "renderer.h"
#include "map.h"
#include "texture.h"
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

float Renderer::aspect() const {
    return (float)width / (float)height;
}

void Renderer::setAtmosphere(int preset) {
    atmoPreset = ((preset % ATMO_COUNT) + ATMO_COUNT) % ATMO_COUNT;
    hazeCool = 0.0f;
    switch (atmoPreset) {
    case ATMO_OVERCAST:   // DayZ gloom: weak grey sun, flat light, close haze
        sunDir        = glm::normalize(glm::vec3(0.35f, 0.75f, 0.30f));
        sunColor      = {0.38f, 0.40f, 0.43f};
        skyZenith     = {0.44f, 0.48f, 0.53f};
        skyHorizon    = {0.63f, 0.66f, 0.69f};
        groundAmbient = {0.33f, 0.34f, 0.35f};
        fogDist = 1300.0f; fogHeightAmt = 1.30f; cloudAmount = 0.55f;
        exposure = 1.05f; saturation = 0.80f;
        break;
    case ATMO_GOLDEN:     // golden hour: low warm sun, long shadows, amber horizon
        sunDir        = glm::normalize(glm::vec3(0.75f, 0.28f, 0.42f));
        sunColor      = {1.25f, 0.85f, 0.52f};
        skyZenith     = {0.34f, 0.44f, 0.66f};
        skyHorizon    = {0.94f, 0.78f, 0.58f};
        groundAmbient = {0.30f, 0.26f, 0.22f};
        fogDist = 2800.0f; fogHeightAmt = 0.60f; cloudAmount = 0.25f;
        exposure = 1.10f; saturation = 1.06f;
        if (gMapId == MAP_LOBBY) {
            sunDir     = glm::normalize(glm::vec3(0.76f, 0.27f, -0.50f));
            sunColor   = {1.30f, 0.95f, 0.62f};
            skyHorizon = {0.90f, 0.80f, 0.66f};
            fogDist = 1250.0f; fogHeightAmt = 0.90f;
            hazeCool = 1.0f;
        }
        break;
    default:              // ATMO_CLEAR: PUBG bright midday (original palette)
        sunDir        = glm::normalize(glm::vec3(0.50f, 0.65f, 0.25f));
        sunColor      = {1.00f, 0.96f, 0.88f};
        skyZenith     = {0.30f, 0.50f, 0.78f};
        skyHorizon    = {0.74f, 0.82f, 0.90f};
        groundAmbient = {0.26f, 0.27f, 0.24f};
        fogDist = 4500.0f; fogHeightAmt = 0.35f; cloudAmount = 0.20f;
        exposure = 0.82f; saturation = 1.12f;
        break;
    }
}

void Renderer::beginFrame(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye) {
    active = &shader;
    curView = view;
    curProj = proj;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    shader.use();
    shader.setMat4(shader.locView, view);
    shader.setMat4(shader.locProj, proj);
    shader.setFloat(shader.locAlpha, 1.0f);
    shader.setInt(shader.locLit, 1);
    shader.setFloat(shader.locClipWater,reflectionPass ? TRAINING_POND_Y : 0);
    shader.setVec3(shader.locEye, eye);
    shader.setInt(shader.locDiffuse, 0);
    shader.setInt(shader.locUseFacade, 0);   // facade UV sampling off except for buildings
    shader.setFloat(shader.locTime, frameTime);
    shader.setInt(shader.locGrass, 0);
    shader.setVec3(shader.locSunDir, sunDir);
    shader.setVec3(shader.locSkyZenith, skyZenith);
    shader.setVec3(shader.locSkyHorizon, skyHorizon);
    shader.setVec3(shader.locGroundAmb, groundAmbient);
    shader.setVec3(shader.locSunColor, sunColor);
    shader.setFloat(shader.locFogDist, fogDist);
    shader.setFloat(shader.locFogHeight, fogHeightAmt);
    shader.setFloat(shader.locCloud, cloudAmount);
    shader.setFloat(shader.locExposure, exposure);
    shader.setFloat(shader.locSaturation, saturation);
    shader.setFloat(shader.locHazeCool, hazeCool);
    shader.setInt(shader.locHasNormal, 0);   // boxes/ground use derivative normals
    // Terrain splat off by default; rock/dirt samplers live on units 2 and 3.
    shader.setInt(shader.locSplat, 0);
    shader.setInt(shader.locRockMap, 2);
    shader.setInt(shader.locDirtMap, 3);
    shader.setInt(shader.locForestMap, 4);
    // Shadow map (built this frame in the shadow pass) on texture unit 1.
    shader.setMat4(shader.locLightSpace, lightSpace);
    shader.setInt(shader.locShadowMap, 1);
    shader.setInt(shader.locUseShadow, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, materials.mats[MAT_GROUND].tex);
}

void Renderer::drawSky(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye) {
    // Fullscreen gradient + clouds, drawn before world geometry. Depth off so it
    // never occludes (and is never occluded by) the scene; world draws over it.
    glm::mat4 invVP = glm::inverse(proj * view);
    glDisable(GL_DEPTH_TEST);
    skyShader.use();
    skyShader.setMat4(skyShader.locInvVP, invVP);
    skyShader.setVec3(skyShader.locEye, eye);
    skyShader.setVec3(skyShader.locSunDir, sunDir);
    skyShader.setVec3(skyShader.locSkyZenith, skyZenith);
    skyShader.setVec3(skyShader.locSkyHorizon, skyHorizon);
    skyShader.setVec3(skyShader.locSunColor, sunColor);
    skyShader.setFloat(skyShader.locTime, frameTime);
    skyShader.setFloat(skyShader.locCloud, cloudAmount);
    skyShader.setFloat(skyShader.locExposure, exposure);
    skyShader.setFloat(skyShader.locSaturation, saturation);
    skyShader.setFloat(skyShader.locHazeCool, hazeCool);
    skyShader.setFloat(skyPanoramaTurnLoc, 0.06f - atan2f(sunDir.z, sunDir.x) / 6.2831853f);
    skyShader.setInt(skyPanoramaLoc,7);
    skyShader.setInt(skyUsePanoramaLoc,gMapId==MAP_LOBBY && meadowSky && atmoPreset!=ATMO_OVERCAST);
    glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D,meadowSky);
    glActiveTexture(GL_TEXTURE0);
    quad2d.draw();
    glEnable(GL_DEPTH_TEST);
    shader.use();   // restore the world program for the geometry that follows
}

void Renderer::beginShadowPass(const glm::vec3& focus) {
    const float R = gMapId==MAP_LOBBY ? 110.0f : 60.0f;          // half-extent of the shadowed region around focus
    const float backDist = 120.0f;  // how far up the sun-ray the light camera sits
    glm::vec3 dir = glm::normalize(sunDir);
    glm::vec3 leye = focus + dir * backDist;
    glm::vec3 up = (dir.y > 0.99f || dir.y < -0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 lview = glm::lookAt(leye, focus, up);
    glm::mat4 lproj = glm::ortho(-R, R, -R, R, 1.0f, backDist + R + 50.0f);
    lightSpace = lproj * lview;
    shadowFocus = focus;

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glViewport(0, 0, shadowSize, shadowSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);   // single-sided ground/terrain must cast; bias handles acne
    depthShader.use();
    depthShader.setMat4(depthShader.locLightSpace, lightSpace);
    active = &depthShader;
}

void Renderer::endShadowPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW, fbH);
    glEnable(GL_CULL_FACE);
    active = &shader;
    shader.use();
}

