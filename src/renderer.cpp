#include "renderer.h"
#include "map.h"
#include "texture.h"
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

bool Renderer::init(const char* title, int w, int h) {
    width = w;
    height = h;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);   // planar mirror reflection mask

    window = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_SetSwapInterval(0);  // VSync off: uncapped frame rate

    if (!loadGLFunctions()) return false;

    int dw, dh;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    glViewport(0, 0, dw, dh);
    fbW = dw;
    fbH = dh;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    char base[512];
    const char* sdlBase = SDL_GetBasePath();
    char vpath[600], fpath[600];
    snprintf(base, sizeof(base), "%s", sdlBase ? sdlBase : "");
    snprintf(vpath, sizeof(vpath), "%sshaders/basic.vert", base);
    snprintf(fpath, sizeof(fpath), "%sshaders/basic.frag", base);
    if (!shader.load(vpath, fpath)) {
        // fallback: run from source tree, shaders/ in cwd
        if (!shader.load("shaders/basic.vert", "shaders/basic.frag")) return false;
    }

    char skyv[600], skyf[600];
    snprintf(skyv, sizeof(skyv), "%sshaders/sky.vert", base);
    snprintf(skyf, sizeof(skyf), "%sshaders/sky.frag", base);
    if (!skyShader.load(skyv, skyf)) {
        if (!skyShader.load("shaders/sky.vert", "shaders/sky.frag")) return false;
    }

    char dpv[600], dpf[600];
    snprintf(dpv, sizeof(dpv), "%sshaders/depth.vert", base);
    snprintf(dpf, sizeof(dpf), "%sshaders/depth.frag", base);
    if (!depthShader.load(dpv, dpf)) {
        if (!depthShader.load("shaders/depth.vert", "shaders/depth.frag")) return false;
    }

    char txv[600], txf[600];
    snprintf(txv, sizeof(txv), "%sshaders/texquad.vert", base);
    snprintf(txf, sizeof(txf), "%sshaders/texquad.frag", base);
    if (!texShader.load(txv, txf)) {
        if (!texShader.load("shaders/texquad.vert", "shaders/texquad.frag")) return false;
    }
    texUvCenterLoc = glGetUniformLocation(texShader.program, "uvCenter");
    texUvHalfLoc   = glGetUniformLocation(texShader.program, "uvHalf");

    if (!createUnitCube(cube)) return false;
    if (!createGroundQuad(ground)) return false;
    // Heightfield mesh: setMap must have run first (pads + terrain mode set) so the
    // mesh matches the ground the server simulates.
    if (!createTerrainMesh(terrain, PALDISKI_HALF, terrainHeight)) return false;
    if (!createQuad2D(quad2d)) return false;
    if (!font.init()) return false;
    if (!materials.init()) return false;
    if (!foliage.init()) return false;
    if (!props.init()) return false;
    if (!grass.init()) return false;

    // Shadow map: a depth-only texture rendered from the sun each frame.
    glGenFramebuffers(1, &shadowFBO);
    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Clamp to a white border so anything sampled outside the map reads "lit" (depth 1).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
    glDrawBuffer(GL_NONE);   // no color attachment
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "shadow FBO incomplete\n");
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    active = &shader;
    return true;
}

float Renderer::aspect() const {
    return (float)width / (float)height;
}

void Renderer::setAtmosphere(int preset) {
    atmoPreset = ((preset % ATMO_COUNT) + ATMO_COUNT) % ATMO_COUNT;
    switch (atmoPreset) {
    case ATMO_OVERCAST:   // DayZ gloom: weak grey sun, flat light, close haze
        sunDir        = glm::normalize(glm::vec3(0.35f, 0.75f, 0.30f));
        sunColor      = {0.38f, 0.40f, 0.43f};
        skyZenith     = {0.44f, 0.48f, 0.53f};
        skyHorizon    = {0.63f, 0.66f, 0.69f};
        groundAmbient = {0.33f, 0.34f, 0.35f};
        fogDist = 220.0f; fogHeightAmt = 1.30f; cloudAmount = 0.55f;
        exposure = 1.05f; saturation = 0.80f;
        break;
    case ATMO_GOLDEN:     // golden hour: low warm sun, long shadows, amber horizon
        sunDir        = glm::normalize(glm::vec3(0.75f, 0.28f, 0.42f));
        sunColor      = {1.25f, 0.85f, 0.52f};
        skyZenith     = {0.34f, 0.44f, 0.66f};
        skyHorizon    = {0.94f, 0.78f, 0.58f};
        groundAmbient = {0.30f, 0.26f, 0.22f};
        fogDist = 300.0f; fogHeightAmt = 0.60f; cloudAmount = 0.25f;
        exposure = 1.10f; saturation = 1.06f;
        break;
    default:              // ATMO_CLEAR: PUBG bright midday (original palette)
        sunDir        = glm::normalize(glm::vec3(0.50f, 0.65f, 0.25f));
        sunColor      = {1.00f, 0.96f, 0.88f};
        skyZenith     = {0.30f, 0.50f, 0.78f};
        skyHorizon    = {0.74f, 0.82f, 0.90f};
        groundAmbient = {0.26f, 0.27f, 0.24f};
        fogDist = 350.0f; fogHeightAmt = 0.35f; cloudAmount = 0.20f;
        exposure = 0.82f; saturation = 1.12f;
        break;
    }
}

void Renderer::beginFrame(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye) {
    active = &shader;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    shader.use();
    shader.setMat4(shader.locView, view);
    shader.setMat4(shader.locProj, proj);
    shader.setFloat(shader.locAlpha, 1.0f);
    shader.setInt(shader.locLit, 1);
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
}

void Renderer::drawSky(const glm::mat4& view, const glm::mat4& proj) {
    // Fullscreen gradient, drawn before world geometry. Depth off so it never
    // occludes (and is never occluded by) the scene; world draws over it normally.
    glm::mat4 invVP = glm::inverse(proj * view);
    glDisable(GL_DEPTH_TEST);
    skyShader.use();
    skyShader.setMat4(skyShader.locInvVP, invVP);
    skyShader.setVec3(skyShader.locSunDir, sunDir);
    skyShader.setVec3(skyShader.locSkyZenith, skyZenith);
    skyShader.setVec3(skyShader.locSkyHorizon, skyHorizon);
    skyShader.setVec3(skyShader.locSunColor, sunColor);
    skyShader.setFloat(skyShader.locExposure, exposure);
    skyShader.setFloat(skyShader.locSaturation, saturation);
    quad2d.draw();
    glEnable(GL_DEPTH_TEST);
    shader.use();   // restore the world program for the geometry that follows
}

void Renderer::beginShadowPass(const glm::vec3& focus) {
    const float R = 60.0f;          // half-extent of the shadowed region around focus
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

static void bindFlatColor(Shader& sh) {
    sh.setInt(sh.locUseTex, 0);
}

static void bindMaterial(Shader& sh, const MaterialLib& lib, MaterialId id) {
    const Material& m = lib.mats[(int)id];
    lib.bind(id);
    sh.setInt(sh.locUseTex, 1);
    sh.setFloat(sh.locTile, m.tile);
    sh.setFloat(sh.locSpec, m.spec);
    sh.setVec3(sh.locTint, m.tint);
}

void Renderer::beginHUD() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    shader.setMat4(shader.locView, glm::mat4(1.0f));
    shader.setMat4(shader.locProj, glm::mat4(1.0f));
    shader.setInt(shader.locLit, 0);
}

void Renderer::drawRect(const glm::vec2& center, const glm::vec2& size,
                        const glm::vec3& color, float alpha) {
    shader.use();  // drawText may have bound the text program
    bindFlatColor(shader);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
    model = glm::scale(model, glm::vec3(size, 1.0f));
    shader.setMat4(shader.locModel, model);
    shader.setVec3(shader.locColor, color);
    shader.setFloat(shader.locAlpha, alpha);
    quad2d.draw();
}

void Renderer::drawRectRot(const glm::vec2& center, const glm::vec2& size,
                           const glm::vec3& color, float alpha, float angle) {
    shader.use();
    bindFlatColor(shader);
    float ia = 1.0f / aspect();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
    model = glm::scale(model, glm::vec3(ia, 1.0f, 1.0f));            // square space -> NDC
    model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));  // rotate in square space
    model = glm::scale(model, glm::vec3(size, 1.0f));
    shader.setMat4(shader.locModel, model);
    shader.setVec3(shader.locColor, color);
    shader.setFloat(shader.locAlpha, alpha);
    quad2d.draw();
}

void Renderer::drawTexQuad(const glm::vec2& center, const glm::vec2& size, unsigned int tex,
                           float alpha, const glm::vec2& uvCenter, const glm::vec2& uvHalf,
                           const glm::vec3& tint) {
    texShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    texShader.setInt(texShader.locDiffuse, 0);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
    model = glm::scale(model, glm::vec3(size, 1.0f));
    texShader.setMat4(texShader.locModel, model);
    texShader.setFloat(texShader.locAlpha, alpha);
    texShader.setVec3(texShader.locTint, tint);
    glUniform2f(texUvCenterLoc, uvCenter.x, uvCenter.y);
    glUniform2f(texUvHalfLoc, uvHalf.x, uvHalf.y);
    quad2d.draw();
    shader.use();   // restore flat program for following drawRect/drawText
}

unsigned int Renderer::mapTexture(int mapId, const Box* boxes, int count, float worldHalf) {
    if (mapId < 0 || mapId > 2) return 0;
    if (!mapTexTried[mapId]) {
        mapTexTried[mapId] = true;
        static const char* names[3] = {"training", "warehouse", "field"};
        char path[64];
        snprintf(path, sizeof(path), "textures/map_%s.png", names[mapId]);
        GLuint t = loadTexture(path);                       // hand-made art if present
        if (!t) t = makeMapTexture(boxes, count, worldHalf); // else procedural bake
        if (t) {   // clamp so corner-minimap sub-rect sampling doesn't wrap at edges
            glBindTexture(GL_TEXTURE_2D, t);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        mapTex[mapId] = t;
    }
    return mapTex[mapId];
}

void Renderer::drawText(const char* s, float x, float y, float h,
                        const glm::vec3& color, float alpha) {
    font.draw(s, x, y, h, 1.0f / aspect(), color, alpha);
}

float Renderer::textWidth(const char* s, float h) const {
    return font.width(s, h, (float)height / (float)width);
}

void Renderer::endHUD() {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    shader.use();
    shader.setFloat(shader.locAlpha, 1.0f);
}

void Renderer::drawCube(const glm::vec3& center, const glm::vec3& scale, const glm::vec3& color) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model = glm::scale(model, scale);
    bindFlatColor(*active);
    active->setMat4(active->locModel, model);
    active->setVec3(active->locColor, color);
    cube.draw();
}

void Renderer::drawCube(const glm::vec3& center, const glm::vec3& scale, MaterialId mat) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model = glm::scale(model, scale);
    bindMaterial(*active, materials, mat);
    active->setMat4(active->locModel, model);
    active->setVec3(active->locColor, glm::vec3(1.0f));
    cube.draw();
}

void Renderer::drawCubeModel(const glm::mat4& model, const glm::vec3& color) {
    bindFlatColor(*active);
    active->setMat4(active->locModel, model);
    active->setVec3(active->locColor, color);
    cube.draw();
}

void Renderer::drawCubeModelTranslucent(const glm::mat4& model, const glm::vec3& color,
                                        float alpha) {
    bindFlatColor(*active);
    active->setMat4(active->locModel, model);
    active->setVec3(active->locColor, color);
    active->setFloat(active->locAlpha, alpha);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    cube.draw();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    active->setFloat(active->locAlpha, 1.0f);
}


void Renderer::invalidateWorldOnMapChange() {
    if (worldBuiltFor == (int)gMapId) return;
    worldBuiltFor = (int)gMapId;
    terrain.destroy();
    createTerrainMesh(terrain, gArenaHalf, terrainHeight);
    townBuilt = false;        // town meshes rebuild from the new gTownBuildings
    foliage.clear();          // scatters regenerate lazily (paldiski only)
    props.clear();
    grass.clear();            // ring rebuilds on the next draw
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
    if (gMapId != MAP_PALDISKI) return;   // the lobby pad has no sea
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

void Renderer::drawTerrain() {
    bindMaterial(*active, materials, MAT_GROUND);   // grass layer on unit 0
    // Rock + dirt layers for the slope/height splat on units 2 and 3.
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, materials.mats[MAT_ROCK].tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, materials.mats[MAT_DIRT].tex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, materials.forestGroundTex);
    glActiveTexture(GL_TEXTURE0);
    active->setFloat(active->locRockTile, materials.mats[MAT_ROCK].tile);
    active->setFloat(active->locDirtTile, materials.mats[MAT_DIRT].tile);
    active->setFloat(active->locForestTile, 3.2f);
    active->setInt(active->locSplat, 1);
    active->setMat4(active->locModel, glm::mat4(1.0f));
    active->setVec3(active->locColor, glm::vec3(1.0f));
    active->setInt(active->locGrass, materials.groundHasImage ? 0 : 1);
    active->setInt(active->locHasNormal, 1);   // smooth analytic heightfield normals
    terrain.draw();
    active->setInt(active->locHasNormal, 0);
    active->setInt(active->locGrass, 0);
    active->setInt(active->locSplat, 0);
}

void Renderer::buildTownMeshes() {
    for (auto& d : townDraws) { d.wall.destroy(); d.decals.destroy(); }
    townDraws.clear();
    if (!facadeTex) facadeTex = makeFacadeTexture();
    if (!decalTex) decalTex = loadTextureRGBA("textures/environment/baltic_decal_atlas_1024.png");
    for (int i = 0; i < gTownBuildingCount; i++) {
        const TownBuilding& b = gTownBuildings[i];
        TownDraw d;
        if (!buildFacadeMesh(d.wall, b)) continue;
        buildBuildingDecalMesh(d.decals, b, i);
        d.center    = b.center;   // y = pad height; the facade mesh base is y=0
        // Concrete parapet cap: a thin slab overhanging the walls at roof height.
        d.capCenter = {b.center.x, b.center.y + b.h + 0.15f, b.center.z};
        d.capScale  = {b.w + 0.5f, 0.3f, b.d + 0.5f};
        // Whole-building AABB (incl. cap) for per-frame frustum culling.
        d.cullCenter = {b.center.x, b.center.y + b.h * 0.5f, b.center.z};
        d.cullHalf   = {b.w * 0.5f + 0.25f, b.h * 0.5f + 0.3f, b.d * 0.5f + 0.25f};
        townDraws.push_back(std::move(d));
    }
    townBuilt = true;
}

void Renderer::drawTownWorld(const Frustum& fr, const glm::vec3& eye) {
    if (!townBuilt) buildTownMeshes();
    // LOD: beyond this distance a building drops its UV facade (texture + tangent
    // normal map) and renders as a plain concrete box — same silhouette, far cheaper
    // fragment work. Fog softens the swap so the transition is hard to spot.
    constexpr float FACADE_LOD_DIST = 120.0f;
    glActiveTexture(GL_TEXTURE0);
    for (const auto& d : townDraws) {
        if (!fr.aabbVisible(d.cullCenter, d.cullHalf)) continue;
        float dist = glm::length(d.cullCenter - eye);
        if (dist > FACADE_LOD_DIST) {
            // Far LOD: solid concrete box (walls), still topped by the roof cap below.
            drawCube(d.cullCenter, d.cullHalf * 2.0f, MAT_CONCRETE);
            drawCube(d.capCenter, d.capScale, MAT_CONCRETE);
            continue;
        }
        // Facade walls: UV-sampled texture (basic program; the facade uniforms are -1
        // on the depth program, so this also draws plain depth in the shadow pass).
        glm::mat4 model = glm::translate(glm::mat4(1.0f), d.center);
        glBindTexture(GL_TEXTURE_2D, facadeTex);
        active->setInt(active->locDiffuse, 0);
        active->setInt(active->locUseFacade, 1);
        active->setInt(active->locUseTex, 0);
        active->setInt(active->locHasNormal, 1);
        active->setFloat(active->locSpec, 0.0f);
        active->setVec3(active->locTint, glm::vec3(1.0f));
        active->setVec3(active->locColor, glm::vec3(1.0f));
        active->setMat4(active->locModel, model);
        d.wall.draw();
        active->setInt(active->locUseFacade, 0);
        active->setInt(active->locHasNormal, 0);
        // Short-range cosmetic overlay only. It is omitted from the depth/shadow
        // program and distant buildings, keeping the cost bounded.
        if (active == &shader && decalTex && dist < 70.0f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glBindTexture(GL_TEXTURE_2D, decalTex);
            active->setInt(active->locUseFacade, 1);
            active->setInt(active->locHasNormal, 1);
            active->setFloat(active->locAlpha, 1.0f);
            d.decals.draw();
            active->setInt(active->locUseFacade, 0);
            active->setInt(active->locHasNormal, 0);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
        // Roof cap (plain concrete cube).
        drawCube(d.capCenter, d.capScale, MAT_CONCRETE);
    }
}

void Renderer::drawFoliage(const glm::mat4& view, const glm::mat4& proj,
                           const glm::vec3& eye, float time) {
    if (foliage.empty()) foliage.generate(gMapId == MAP_LOBBY ? 12 : 2600);
    foliage.draw(*this, view, proj, eye, time);
    shader.use();   // restore world program for subsequent draws (view model, HUD)
}

void Renderer::drawFoliageDepth(float time) {
    if (foliage.empty()) foliage.generate(gMapId == MAP_LOBBY ? 12 : 2600);
    foliage.drawDepth(lightSpace, shadowFocus, time);
    depthShader.use();   // restore the world depth program for the rest of the pass
}

void Renderer::drawProps(const glm::mat4& view, const glm::mat4& proj,
                         const glm::vec3& eye, float time) {
    if (props.empty()) props.generate();
    props.draw(*this, view, proj, eye, time);
    shader.use();   // restore world program for subsequent draws (view model, HUD)
}

void Renderer::drawPropsDepth() {
    if (props.empty()) props.generate();   // shadow pass runs first; scatter here
    props.drawDepth(lightSpace, shadowFocus);
    depthShader.use();   // restore the world depth program for the rest of the pass
}

void Renderer::drawGrass(const glm::mat4& view, const glm::mat4& proj,
                         const glm::vec3& eye, float time) {
    // Dense clump ring on both maps (lobby lawn keeps its firing lane mowed).
    grass.draw(*this, view, proj, eye, time);
    shader.use();   // restore world program for subsequent draws (view model, HUD)
}

void Renderer::endFrame() {
    SDL_GL_SwapWindow(window);
}

void Renderer::toggleWireframe() {
    wireframe = !wireframe;
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
}

void Renderer::shutdown() {
    foliage.destroy();
    props.destroy();
    grass.destroy();
    materials.destroy();
    font.destroy();
    quad2d.destroy();
    terrain.destroy();
    ground.destroy();
    cube.destroy();
    shader.destroy();
    skyShader.destroy();
    depthShader.destroy();
    texShader.destroy();
    for (int i = 0; i < MAP_COUNT; i++) if (mapTex[i]) glDeleteTextures(1, &mapTex[i]);
    for (auto& d : townDraws) d.wall.destroy();
    townDraws.clear();
    if (facadeTex) glDeleteTextures(1, &facadeTex);
    if (decalTex) glDeleteTextures(1, &decalTex);
    if (shadowTex) glDeleteTextures(1, &shadowTex);
    if (shadowFBO) glDeleteFramebuffers(1, &shadowFBO);
    if (glContext) SDL_GL_DeleteContext(glContext);
    if (window) SDL_DestroyWindow(window);
    glContext = nullptr;
    window = nullptr;
}
