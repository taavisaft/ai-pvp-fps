#include "renderer.h"
#include "renderer_bind.h"
#include "map.h"
#include "texture.h"
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>


void Renderer::invalidateWorldOnMapChange() {
    if (worldBuiltFor == (int)gMapId) return;
    worldBuiltFor = (int)gMapId;
    setAtmosphere(atmoPreset);
    veg.invalidate();                   // placements re-scatter for the new map
    taigaTerrain.destroy();
    if(gMapId==MAP_LOBBY) taigaTerrain.prepareTraining();
    veg.prepareMeadow();
}

void Renderer::drawVegetation(const Frustum& fr, const glm::vec3& eye) {
    if (gTerrainMode == TERRAIN_OFF) return;   // lobby gets its miniature ring too
    static bool noVeg = getenv("FPS_NOVEG") != nullptr;   // perf-isolation debug
    if (noVeg) return;
    if (active == &depthShader) {
        // Sun pass (culling already off): near LOD0 trees cast shadows.
        veg.drawShadow(fr, shadowFocus, frameTime, lightSpace);
        depthShader.use();
    } else {
        glDisable(GL_CULL_FACE);   // blades and cone skirts are two-sided
        veg.drawLit(*this, fr, eye);
        glEnable(GL_CULL_FACE);
        shader.use();
    }
}

void Renderer::drawGround() {
    // Flat lobby pad: the 100 m base quad scaled to the arena clamp.
    bindMaterial(*active, materials, MAT_GROUND);
    float s = (gArenaHalf + 5.0f) / 50.0f;
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(s, 1.0f, s));
    active->setMat4(active->locModel, model);
    active->setVec3(active->locColor, glm::vec3(1.0f));
    active->setInt(active->locGrass, materials.groundHasImage ? 0 : 1);
    ground.draw();
    active->setInt(active->locGrass, 0);
}

void Renderer::drawWater() {
    if (gMapId == MAP_LOBBY) { drawPond(); return; }
    // Translucent Baltic at SEA_LEVEL: the 100 m ground quad scaled well past the map
    // edge so the sea runs into the fog on the west horizon. Depth-tested against the
    // terrain (so the shore contact line is exact) but not depth-written, and drawn
    // after all opaque world geometry. Slight specular gives a view-aligned glint.
    const float span = (PALDISKI_HALF * 4.0f) / 50.0f;   // quad half-extent is 50
    glm::mat4 model = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, SEA_LEVEL, 0.0f)),
        glm::vec3(span, 1.0f, span));
    shader.use();
    bindFlatColor(shader);
    shader.setMat4(shader.locModel, model);
    shader.setVec3(shader.locColor, glm::vec3(0.13f, 0.22f, 0.28f));
    shader.setFloat(shader.locAlpha, 0.82f);
    shader.setFloat(shader.locSpec, 0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    ground.draw();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    shader.setFloat(shader.locAlpha, 1.0f);
    shader.setFloat(shader.locSpec, 0.0f);
}

void Renderer::drawTerrain(const Frustum& fr, const glm::vec3& eye) {
    bindMaterial(*active, materials, MAT_GROUND);   // grass layer on unit 0
    // Rock + dirt layers for the slope/height splat on units 2 and 3.
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, materials.mats[MAT_ROCK].tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, materials.mats[MAT_DIRT].tex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, gMapId == MAP_LOBBY && materials.trainingGroundTex
                  ? materials.trainingGroundTex : materials.forestGroundTex);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, gMapId == MAP_LOBBY ? veg.landscapeTex : 0);
    glActiveTexture(GL_TEXTURE0);
    active->setFloat(active->locRockTile, materials.mats[MAT_ROCK].tile);
    active->setFloat(active->locDirtTile, materials.mats[MAT_DIRT].tile);
    active->setFloat(active->locForestTile, 3.2f);
    active->setInt(active->locSplat, gMapId == MAP_LOBBY ? 2 : 1);
    active->setMat4(active->locModel, glm::mat4(1.0f));
    active->setVec3(active->locColor, glm::vec3(1.0f));
    // Use textures/ground.* when present (triplanar splat base layer). Procedural
    // grassColor() only when no ground image was loaded.
    bool procGrass = !materials.groundHasImage;
    active->setInt(active->locGrass, procGrass ? 1 : 0);
    active->setInt(active->locHasNormal, 1);   // smooth analytic heightfield normals
    if (gTerrainMode != TERRAIN_OFF) {
        // Chunked LOD ground + the mountain vista ring (vista only in the lit pass:
        // the sun frustum never reaches it, and its shadows would be wrong anyway).
        taigaTerrain.draw(fr, eye, /*withVista=*/active == &shader && gMapId==MAP_PALDISKI,
                          reflectionPass ? 1 : 0);
    } else {
        terrain.draw();   // lobby: one small static mesh
    }
    active->setInt(active->locHasNormal, 0);
    active->setInt(active->locGrass, 0);
    active->setInt(active->locSplat, 0);
}

