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

    if (!createUnitCube(cube)) return false;
    if (!createGroundQuad(ground)) return false;
    if (!createTerrainMesh(terrain)) return false;
    if (!createQuad2D(quad2d)) return false;
    if (!font.init()) return false;
    if (!materials.init()) return false;
    return true;
}

float Renderer::aspect() const {
    return (float)width / (float)height;
}

void Renderer::beginFrame(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye) {
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
    bindFlatColor(shader);
    shader.setMat4(shader.locModel, model);
    shader.setVec3(shader.locColor, color);
    cube.draw();
}

void Renderer::drawCube(const glm::vec3& center, const glm::vec3& scale, MaterialId mat) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model = glm::scale(model, scale);
    bindMaterial(shader, materials, mat);
    shader.setMat4(shader.locModel, model);
    shader.setVec3(shader.locColor, glm::vec3(1.0f));
    cube.draw();
}

void Renderer::drawCubeModel(const glm::mat4& model, const glm::vec3& color) {
    bindFlatColor(shader);
    shader.setMat4(shader.locModel, model);
    shader.setVec3(shader.locColor, color);
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
    bindMaterial(shader, materials, mat);
    shader.setMat4(shader.locModel, glm::mat4(1.0f));
    shader.setVec3(shader.locColor, glm::vec3(1.0f));
    // Use the procedural grass shader only when no ground image is loaded; otherwise
    // the triplanar path samples the grass photo (textures/ground.*).
    shader.setInt(shader.locGrass, materials.groundHasImage ? 0 : 1);
    ground.draw();
    shader.setInt(shader.locGrass, 0);
}

void Renderer::drawTerrain() {
    bindMaterial(shader, materials, MAT_GROUND);
    shader.setMat4(shader.locModel, glm::mat4(1.0f));
    shader.setVec3(shader.locColor, glm::vec3(1.0f));
    shader.setInt(shader.locGrass, materials.groundHasImage ? 0 : 1);
    terrain.draw();
    shader.setInt(shader.locGrass, 0);
}

void Renderer::endFrame() {
    SDL_GL_SwapWindow(window);
}

void Renderer::toggleWireframe() {
    wireframe = !wireframe;
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
}

void Renderer::shutdown() {
    materials.destroy();
    font.destroy();
    quad2d.destroy();
    terrain.destroy();
    ground.destroy();
    cube.destroy();
    shader.destroy();
    if (glContext) SDL_GL_DeleteContext(glContext);
    if (window) SDL_DestroyWindow(window);
    glContext = nullptr;
    window = nullptr;
}
