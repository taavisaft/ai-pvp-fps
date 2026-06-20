#include "renderer.h"
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

    if (!createUnitCube(cube)) return false;
    if (!createGroundQuad(ground)) return false;
    if (!createTerrainMesh(terrain)) return false;
    if (!createQuad2D(quad2d)) return false;
    if (!font.init()) return false;
    if (!materials.init()) return false;
    if (!foliage.init()) return false;

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
    shader.setFloat(shader.locTime, frameTime);
    shader.setInt(shader.locGrass, 0);
    shader.setVec3(shader.locSunDir, sunDir);
    shader.setVec3(shader.locSkyZenith, skyZenith);
    shader.setVec3(shader.locSkyHorizon, skyHorizon);
    shader.setVec3(shader.locGroundAmb, groundAmbient);
    shader.setInt(shader.locHasNormal, 0);   // boxes/ground use derivative normals
    // Terrain splat off by default; rock/dirt samplers live on units 2 and 3.
    shader.setInt(shader.locSplat, 0);
    shader.setInt(shader.locRockMap, 2);
    shader.setInt(shader.locDirtMap, 3);
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

void Renderer::setView(const glm::mat4& view) {
    shader.use();
    shader.setMat4(shader.locView, view);
}

void Renderer::fillDepthFar() {
    // Full-screen quad at the far plane (identity view/proj, z=1 -> depth 1). Used with
    // a stencil test to reset depth inside the mirror so reflected geometry (which lives
    // behind the wall in world space) draws instead of being occluded by it. Caller sets
    // the stencil/colormask/depth-func state; this only issues the geometry.
    shader.use();
    bindFlatColor(shader);
    shader.setMat4(shader.locView, glm::mat4(1.0f));
    shader.setMat4(shader.locProj, glm::mat4(1.0f));
    glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 1.0f)),
                             glm::vec3(2.0f, 2.0f, 1.0f));
    shader.setMat4(shader.locModel, m);
    quad2d.draw();
}

void Renderer::drawGround() {
    drawGround(MAT_GROUND);
}

void Renderer::drawGround(MaterialId mat) {
    bindMaterial(*active, materials, mat);
    active->setMat4(active->locModel, glm::mat4(1.0f));
    active->setVec3(active->locColor, glm::vec3(1.0f));
    // Use the procedural grass shader only when no ground image is loaded; otherwise
    // the triplanar path samples the grass photo (textures/ground.*).
    active->setInt(active->locGrass, materials.groundHasImage ? 0 : 1);
    ground.draw();
    active->setInt(active->locGrass, 0);
}

void Renderer::drawTerrain() {
    bindMaterial(*active, materials, MAT_GROUND);   // grass layer on unit 0
    // Rock + dirt layers for the slope/height splat on units 2 and 3.
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, materials.mats[MAT_ROCK].tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, materials.mats[MAT_DIRT].tex);
    glActiveTexture(GL_TEXTURE0);
    active->setFloat(active->locRockTile, materials.mats[MAT_ROCK].tile);
    active->setFloat(active->locDirtTile, materials.mats[MAT_DIRT].tile);
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

void Renderer::drawFoliage(const glm::mat4& view, const glm::mat4& proj,
                           const glm::vec3& eye, float time) {
    if (gMapId != MAP_FIELD) {            // forest only exists on the open field map
        if (!foliage.empty()) foliage.clear();
        return;
    }
    if (foliage.empty()) foliage.generate(1800);
    foliage.draw(*this, view, proj, eye, time);
    shader.use();   // restore world program for subsequent draws (view model, HUD)
}

void Renderer::drawFoliageDepth(float time) {
    if (gMapId != MAP_FIELD) { if (!foliage.empty()) foliage.clear(); return; }
    if (foliage.empty()) foliage.generate(1800);   // shadow pass runs first; scatter here
    foliage.drawDepth(lightSpace, time);
    depthShader.use();   // restore the world depth program for the rest of the pass
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
    materials.destroy();
    font.destroy();
    quad2d.destroy();
    terrain.destroy();
    ground.destroy();
    cube.destroy();
    shader.destroy();
    skyShader.destroy();
    depthShader.destroy();
    if (shadowTex) glDeleteTextures(1, &shadowTex);
    if (shadowFBO) glDeleteFramebuffers(1, &shadowFBO);
    if (glContext) SDL_GL_DeleteContext(glContext);
    if (window) SDL_DestroyWindow(window);
    glContext = nullptr;
    window = nullptr;
}
