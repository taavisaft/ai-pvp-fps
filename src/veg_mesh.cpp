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

// One grass instance = a tuft of three tapered blades fanned around the root
// (3x the visual density per instance for the same instance-stream cost).
// Up-facing normals on purpose: blades then take exactly the ground's lighting
// and read as part of the field instead of dark spikes on it.
void vegBuildBlade(std::vector<float>& v, std::vector<unsigned>& idx) {
    const glm::vec3 root(0.105f, 0.190f, 0.070f);
    const glm::vec3 tip (0.300f, 0.430f, 0.150f);
    const glm::vec3 n(0.0f, 1.0f, 0.0f);
    struct Row { float y, w, bend, flex; };
    const Row rows[3] = {{0.00f, 0.026f, 0.00f, 0.0f},
                         {0.22f, 0.019f, 0.03f, 0.3f},
                         {0.37f, 0.011f, 0.08f, 0.65f}};
    const float bladeYaw[3]  = {0.0f, 2.19f, 4.35f};   // fan directions
    const float bladeSize[3] = {1.0f, 0.78f, 0.62f};
    for (int b = 0; b < 3; b++) {
        float cy = cosf(bladeYaw[b]), sy = sinf(bladeYaw[b]);
        float sc = bladeSize[b];
        glm::vec3 off(cy * 0.035f, 0.0f, sy * 0.035f);   // root offset from center
        auto place = [&](float x, float y, float z) {
            glm::vec3 p = glm::vec3(cy * x - sy * z, y, sy * x + cy * z) * sc + off;
            return p;
        };
        unsigned r[3][2];
        for (int i = 0; i < 3; i++) {
            glm::vec3 c = root + (tip - root) * (rows[i].y / 0.48f);
            r[i][0] = pushV(v, place(-rows[i].w, rows[i].y, rows[i].bend), n, c, rows[i].flex);
            r[i][1] = pushV(v, place( rows[i].w, rows[i].y, rows[i].bend), n, c, rows[i].flex);
        }
        unsigned t = pushV(v, place(0.0f, 0.48f, 0.15f), n, tip, 1.0f);
        for (int i = 0; i < 2; i++)
            idx.insert(idx.end(), {r[i][0], r[i][1], r[i + 1][1],
                                   r[i][0], r[i + 1][1], r[i + 1][0]});
        idx.insert(idx.end(), {r[2][0], r[2][1], t});
    }
}

// Textured spruce, unit height 1 (instance scale = tree height in meters).
// DayZ-style construction: a tapered bark trunk plus branch "cards" — flat quads
// carrying a photo of a real needle spray (textures/spruce_branch.png, alpha
// cutout). Each branch is an X-pair: two quads crossing along the branch spine,
// rolled +-35 deg, so a branch shows area from every view direction (one flat
// quad vanishes edge-on). Cards sit in whorls like a real spruce; crown diameter
// ~0.35 x height.
// `low` is currently ignored: the card tree is ~4x cheaper than the old solid
// cone was, and any geometric difference between LOD0 and LOD1 shows up in the
// dither cross-fade band as a half-dissolved ghost tree (cards are sparse — a
// dropped card has nothing to fade against). One mesh, zero ghosting; split the
// LODs again only if vertex cost ever shows up in a profile.
void vegBuildSpruce(std::vector<float>& v, std::vector<unsigned>& idx, bool low) {
    (void)low;
    const glm::vec3 bark(0.320f, 0.235f, 0.165f);
    const glm::vec3 up(0, 1, 0);

    // Trunk: tapered capped prism, full height (visible between the card whorls).
    {
        const int ts = TREE_TRUNK_SIDES;
        unsigned b[8], t[8];
        for (int i = 0; i < ts; i++) {
            float a = (float)i / ts * 6.2831853f;
            glm::vec3 d(cosf(a), 0.0f, sinf(a));
            glm::vec3 c = bark * (0.85f + 0.3f * ghash((float)i, 3.0f));
            b[i] = pushV(v, d * TREE_TRUNK_BASE,                          d, c, 0.0f);
            t[i] = pushV(v, d * TREE_TRUNK_TOP + glm::vec3(0, TREE_TRUNK_HEIGHT, 0), d, c, 0.0f);
        }
        for (int i = 0; i < ts; i++) {
            int j = (i + 1) % ts;
            idx.insert(idx.end(), {b[i], b[j], t[j], b[i], t[j], t[i]});
        }
        const unsigned bottom = pushV(v, glm::vec3(0), -up, bark, 0.0f);
        const unsigned top = pushV(v, {0, TREE_TRUNK_HEIGHT, 0}, up, bark, 0.0f);
        for (int i = 0; i < ts; ++i) {
            int j = (i + 1) % ts;
            idx.insert(idx.end(), {bottom, b[i], b[j], top, t[j], t[i]});
        }
    }

    // One branch = X-pair of quads along a drooping spine. The spray photo maps
    // with its stem base (v=0) at the trunk, tip pointing outward.
    auto card = [&](glm::vec3 root, glm::vec3 axis, glm::vec3 tang, float len,
                    float halfW, glm::vec3 n, float shade, float flexTip) {
        glm::vec3 tint = glm::vec3(shade);
        for (int q = 0; q < 2; q++) {
            float roll = (q == 0 ? 0.6109f : -0.6109f);   // +-35 deg
            glm::vec3 side = tang * cosf(roll)
                           + glm::normalize(glm::cross(axis, tang)) * sinf(roll);
            side *= halfW;
            glm::vec3 tip = root + axis * len;
            unsigned i0 = pushVT(v, root - side, n, tint, 0.04f, {0, 0});
            unsigned i1 = pushVT(v, root + side, n, tint, 0.04f, {1, 0});
            unsigned i2 = pushVT(v, tip  - side, n, tint, flexTip, {0, 1});
            unsigned i3 = pushVT(v, tip  + side, n, tint, flexTip, {1, 1});
            idx.insert(idx.end(), {i0, i1, i3, i0, i3, i2});
        }
    };

    // Whorls. Lowest at 0.24 * height: a 7-9 m spruce keeps its needles above
    // eye level, so walking through a stand doesn't fill the screen with canopy
    // (and firing lanes exist between the trunks, DayZ-style).
    const int whorls   = 14;
    const int branches = 4;
    for (int k = 0; k < whorls; k++) {
        float t = (float)k / (whorls - 1);
        float y = 0.24f + 0.66f * t + (ghash((float)k, 51.0f) - 0.5f) * 0.03f;
        // Crown taper: branch length shrinks toward the top; per-branch jitter
        // breaks the perfect cone outline.
        float len   = (0.195f - 0.120f * t);
        float droop = 0.42f - 0.28f * t;   // radians below horizontal, more at base
        for (int b2 = 0; b2 < branches; b2++) {
            float yaw = k * 2.3999f + b2 * (6.2831853f / branches)
                      + (ghash((float)(k * 7 + b2), 13.0f) - 0.5f) * 1.1f;
            glm::vec3 dir(cosf(yaw), 0.0f, sinf(yaw));
            glm::vec3 tang = glm::normalize(glm::cross(up, dir));
            glm::vec3 axis = glm::normalize(dir * cosf(droop) - up * sinf(droop));
            float jl = len * (0.85f + 0.35f * ghash((float)(k * 31 + b2), 7.0f));
            // Cone-shell fake normal (radial + up): flat quads with true normals
            // would each light as one flat sheet — this shades the crown as one
            // smooth cone instead, same trick the old skirt mesh used.
            glm::vec3 n = glm::normalize(dir * 0.55f + up * 0.85f);
            float shade = 1.30f + 0.60f * ghash((float)(k + b2 * 17), 29.0f);
            card(dir * 0.012f + glm::vec3(0, y, 0), axis, tang, jl,
                 jl * 0.40f, n, shade, 0.16f);
        }
    }

    // Leader: upright X-cross at the top, the spray photo standing as the spike.
    {
        float h = 0.24f;
        glm::vec3 root(0, 1.0f - h * 0.88f, 0);
        glm::vec3 n = glm::normalize(glm::vec3(0.3f, 0.9f, 0.1f));
        card(root, up, {1, 0, 0}, h, h * 0.34f, n, 1.15f, 0.20f);
    }
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
bool vegBakeImpostor(Vegetation& veg, int texW, int texH) {
    GLuint fbo = 0, depthRb = 0;
    glGenTextures(1, &veg.impTex);
    glBindTexture(GL_TEXTURE_2D, veg.impTex);
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
                           veg.impTex, 0);
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
        // Ortho box matching impSize: x in +-0.42, y in 0..1.10, viewed from +X.
        glm::mat4 view = glm::lookAt(glm::vec3(4, 0.55f, 0), glm::vec3(0, 0.55f, 0),
                                     glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::ortho(-veg.impSize.x * 0.5f, veg.impSize.x * 0.5f,
                                    -veg.impSize.y * 0.5f, veg.impSize.y * 0.5f,
                                    0.1f, 10.0f);
        veg.vegSh.setMat4(veg.vegSh.locView, view);
        veg.vegSh.setMat4(veg.vegSh.locProj, proj);
        veg.vegSh.setInt(veg.locBake, 1);
        veg.vegSh.setFloat(veg.locWind, 0.0f);
        veg.vegSh.setFloat(veg.locRange, 0.0f);
        glUniform2f(veg.locFadeIn, 0.0f, 0.0f);
        glUniform2f(veg.locFadeOut, 0.0f, 0.0f);
        veg.vegSh.setFloat(veg.vegSh.locTime, 0.0f);
        veg.vegSh.setVec3(veg.vegSh.locEye, glm::vec3(100.0f));
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, veg.branchTex);
        glActiveTexture(GL_TEXTURE0);

        const float inst[8] = {0, 0, 0, 1, 0, 0, 1, 0};
        glBindBuffer(GL_ARRAY_BUFFER, veg.streamL0);
        glBufferData(GL_ARRAY_BUFFER, sizeof(inst), inst, GL_STREAM_DRAW);
        glBindVertexArray(veg.vaoL0);
        glDrawElementsInstanced(GL_TRIANGLES, veg.l0Idx, GL_UNSIGNED_INT, nullptr, 1);
        glBindVertexArray(0);

        veg.vegSh.setInt(veg.locBake, 0);
        glEnable(GL_CULL_FACE);
        glBindTexture(GL_TEXTURE_2D, veg.impTex);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &depthRb);
    glDeleteFramebuffers(1, &fbo);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);   // restore the renderer's clear color
    return ok;
}
