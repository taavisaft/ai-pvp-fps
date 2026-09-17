#include "renderer.h"
#include "renderer_bind.h"
#include "map.h"
#include "texture.h"
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>


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

void Renderer::drawMesh(const Mesh& m, const glm::vec3& pos, MaterialId mat) {
    bindMaterial(*active, materials, mat);
    active->setMat4(active->locModel, glm::translate(glm::mat4(1.0f), pos));
    active->setVec3(active->locColor, glm::vec3(1.0f));
    m.draw();
}

void Renderer::drawCubeModel(const glm::mat4& model, const glm::vec3& color) {
    bindFlatColor(*active);
    active->setMat4(active->locModel, model);
    active->setVec3(active->locColor, color);
    cube.draw();
}
void Renderer::drawMeshModel(const Mesh& m, const glm::mat4& model, const glm::vec3& color) {
    bindFlatColor(*active);
    active->setMat4(active->locModel, model);
    active->setVec3(active->locColor, color);
    if (m.authoredMaterial) {
        active->setInt(active->locHasNormal, 1);
        active->setInt(active->locAuthoredMaterial, 1);
    }
    m.draw();
    if (m.authoredMaterial) {
        active->setInt(active->locAuthoredMaterial, 0);
        active->setInt(active->locHasNormal, 0);
    }
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


