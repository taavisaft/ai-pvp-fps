#include "vegetation.h"
#include "tree_collision.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// ---- GLSL-mirror noise (must match basic.frag hash/vnoise/fbm exactly) --------
static float gfract(float x) { return x - floorf(x); }

static float ghash(float x, float y) {
    x = gfract(x * 123.34f);
    y = gfract(y * 456.21f);
    float d = x * (x + 45.32f) + y * (y + 45.32f);
    x += d; y += d;
    return gfract(x * y);
}

static float gvnoise(float x, float y) {
    float ix = floorf(x), iy = floorf(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = ghash(ix, iy),        b = ghash(ix + 1.0f, iy);
    float c = ghash(ix, iy + 1.0f), d = ghash(ix + 1.0f, iy + 1.0f);
    float ab = a + (b - a) * fx, cd = c + (d - c) * fx;
    return ab + (cd - ab) * fy;
}

float vegFbm(float x, float y) {
    float v = 0.0f, a = 0.5f;
    for (int i = 0; i < 4; i++) {
        v += a * gvnoise(x, y);
        x *= 2.0f; y *= 2.0f; a *= 0.5f;
    }
    return v;
}

// ---- geometry builders --------------------------------------------------------
// Vertex layout: pos(3) normal(3) color(3) flex(1) uv(2) — see vegMakeVAO.
// uv = (-1,-1) marks an untextured vertex (trunk, grass blades): the shader then
// shades from the vertex color alone instead of sampling the branch atlas.
static unsigned pushVT(std::vector<float>& v, glm::vec3 p, glm::vec3 n, glm::vec3 c,
                       float flex, glm::vec2 uv) {
    unsigned i = (unsigned)(v.size() / 12);
    v.insert(v.end(), {p.x, p.y, p.z, n.x, n.y, n.z, c.x, c.y, c.z, flex, uv.x, uv.y});
    return i;
}

static unsigned pushV(std::vector<float>& v, glm::vec3 p, glm::vec3 n, glm::vec3 c,
                      float flex) {
    return pushVT(v, p, n, c, flex, {-1.0f, -1.0f});
}

// Bush, unit height 1, width ~1.5 (instance scale = bush height in meters).
// Same card idea as the spruce but no trunk: two tiers of photo-textured quads
// (textures/bush_1.png) crossed around the root. Upright inner cross gives the
// body, outward-leaning lower ring fills the base silhouette. Up-dominant fake
// normals so undergrowth takes roughly the ground's lighting and sits in the
// field instead of glowing against it (same reasoning as the grass blades).
void vegBuildBush(std::vector<float>& v, std::vector<unsigned>& idx) {
    const glm::vec3 up(0, 1, 0);
    auto quad = [&](glm::vec3 root, glm::vec3 axis, glm::vec3 side, glm::vec3 n,
                    float shade, float flexTip) {
        glm::vec3 tint(shade);
        glm::vec3 tip = root + axis;
        unsigned i0 = pushVT(v, root - side, n, tint, 0.02f, {0, 0});
        unsigned i1 = pushVT(v, root + side, n, tint, 0.02f, {1, 0});
        unsigned i2 = pushVT(v, tip  - side, n, tint, flexTip, {0, 1});
        unsigned i3 = pushVT(v, tip  + side, n, tint, flexTip, {1, 1});
        idx.insert(idx.end(), {i0, i1, i3, i0, i3, i2});
    };
    // Inner tier: three near-vertical quads crossed at 60 deg.
    for (int i = 0; i < 3; i++) {
        float a = (float)i * 2.0944f + ghash((float)i, 91.0f) * 0.5f;
        glm::vec3 d(cosf(a), 0.0f, sinf(a));
        glm::vec3 lean = glm::normalize(up + d * (0.10f + 0.15f * ghash((float)i, 92.0f)));
        glm::vec3 n = up;   // ground lighting, like the blades — never flips dark
        quad({0, -0.02f, 0}, lean * 1.02f, d * 0.75f,
             n, 0.95f + 0.35f * ghash((float)i, 93.0f), 0.30f);
    }
    // Outer tier: three shorter quads leaning outward, yaws offset between the
    // inner ones — thickens the base so the bush reads as a mound, not a fan.
    for (int i = 0; i < 3; i++) {
        float a = (float)i * 2.0944f + 1.0472f + ghash((float)i, 94.0f) * 0.5f;
        glm::vec3 d(cosf(a), 0.0f, sinf(a));
        glm::vec3 lean = glm::normalize(up * 0.8f + d * 0.75f);
        glm::vec3 n = up;
        quad(d * 0.10f + glm::vec3(0, -0.02f, 0), lean * 0.70f,
             glm::vec3(-d.z, 0, d.x) * 0.52f,
             n, 0.85f + 0.35f * ghash((float)i, 95.0f), 0.34f);
    }
}

// VAO wiring shared by every mesh-vegetation draw: 12-float vertices plus an
// 8-float-per-instance stream (two vec4 attribs, divisor 1).
GLuint vegMakeVAO(GLuint vbo, GLuint ebo, GLuint inst) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLsizei stride = 12 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBindBuffer(GL_ARRAY_BUFFER, inst);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
    glVertexAttribDivisor(5, 1);
    glBindVertexArray(0);
    return vao;
}

// Render the LOD0 spruce once into a texture — the far-tree billboard. Bake is
// raw albedo (veg.frag bake=1); the impostor shader lights it at draw time.
bool vegBakeImpostor(Vegetation& veg, int texW, int texH, int trainingType) {
    GLuint& texture=veg.trainingSpruce[trainingType].impostor;
    const glm::vec2 size(trainingTreeWidth(trainingType),1.10f);
    GLuint fbo = 0, depthRb = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texW, texH, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Deep mips average alpha toward zero-ish values that still pass the cutout
    // test as a solid smudge — clamp the chain so far trees keep their silhouette.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &depthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, texW, texH);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              depthRb);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (ok) {
        glViewport(0, 0, texW, texH);
        // Background: needle green at alpha 0, so linear filtering at the cutout
        // edge blends toward foliage instead of black fringes.
        glClearColor(0.08f, 0.13f, 0.07f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_CULL_FACE);

        veg.vegSh.use();
        glm::mat4 view = glm::lookAt(glm::vec3(4, 0.55f, 0), glm::vec3(0, 0.55f, 0),
                                     glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::ortho(-size.x * 0.5f, size.x * 0.5f,
                                    -size.y * 0.5f, size.y * 0.5f,
                                    0.1f, 10.0f);
        veg.vegSh.setMat4(veg.vegSh.locView, view);
        veg.vegSh.setMat4(veg.vegSh.locProj, proj);
        veg.vegSh.setInt(veg.locBake, 1);
        veg.vegSh.setInt(veg.locTrainingTree,trainingType<4 ? 1 : trainingType-2);
        veg.vegSh.setFloat(veg.locWind, 0.0f);
        veg.vegSh.setFloat(veg.locRange, 0.0f);
        glUniform2f(veg.locFadeIn, 0.0f, 0.0f);
        glUniform2f(veg.locFadeOut, 0.0f, 0.0f);
        veg.vegSh.setFloat(veg.vegSh.locTime, 0.0f);
        veg.vegSh.setVec3(veg.vegSh.locEye, glm::vec3(100.0f));
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, trainingType<4 ? veg.trainingBranchTex : veg.trainingBroadleafTex);
        if (veg.shadowTex) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, veg.shadowTex);
        }
        glActiveTexture(GL_TEXTURE0);

        const float inst[8] = {0, 0, 0, 1, 0, 0, 1, 0};
        glBindBuffer(GL_ARRAY_BUFFER, veg.streamL0);
        glBufferData(GL_ARRAY_BUFFER, sizeof(inst), inst, GL_STREAM_DRAW);
        glBindVertexArray(veg.trainingSpruce[trainingType].vao[0]);
        glDrawElementsInstanced(GL_TRIANGLES, veg.trainingSpruce[trainingType].count, GL_UNSIGNED_INT, nullptr, 1);
        glBindVertexArray(0);

        veg.vegSh.setInt(veg.locBake, 0);
        veg.vegSh.setInt(veg.locTrainingTree,0);
        glEnable(GL_CULL_FACE);
        glBindTexture(GL_TEXTURE_2D, texture);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &depthRb);
    glDeleteFramebuffers(1, &fbo);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);   // restore the renderer's clear color
    return ok;
}
