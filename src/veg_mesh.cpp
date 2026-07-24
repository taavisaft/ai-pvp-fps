#include "vegetation.h"
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
// Vertex layout: pos(3) normal(3) color(3) flex(1) — see vegMakeVAO.
static unsigned pushV(std::vector<float>& v, glm::vec3 p, glm::vec3 n, glm::vec3 c,
                      float flex) {
    unsigned i = (unsigned)(v.size() / 10);
    v.insert(v.end(), {p.x, p.y, p.z, n.x, n.y, n.z, c.x, c.y, c.z, flex});
    return i;
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

// Procedural spruce, unit height 1 (instance scale = tree height in meters).
// Drooping ring "skirts" of needles around a tapered trunk; per-vertex radius and
// color jitter breaks the cone silhouette. low=true builds the mid-range LOD.
void vegBuildSpruce(std::vector<float>& v, std::vector<unsigned>& idx, bool low) {
    const int   seg    = low ? 7 : 12;
    const int   skirts = low ? 4 : 7;
    const glm::vec3 bark(0.230f, 0.160f, 0.100f);
    const glm::vec3 needleDark(0.052f, 0.100f, 0.050f);
    const glm::vec3 needleLite(0.110f, 0.200f, 0.085f);

    // Trunk: tapered open prism up to the first skirt.
    {
        const int   ts = low ? 4 : 6;
        unsigned b[8], t[8];
        for (int i = 0; i < ts; i++) {
            float a = (float)i / ts * 6.2831853f;
            glm::vec3 d(cosf(a), 0.0f, sinf(a));
            glm::vec3 c = bark * (0.85f + 0.3f * ghash((float)i, 3.0f));
            b[i] = pushV(v, d * 0.022f,                  d, c, 0.0f);
            t[i] = pushV(v, d * 0.013f + glm::vec3(0, 0.42f, 0), d, c, 0.0f);
        }
        for (int i = 0; i < ts; i++) {
            int j = (i + 1) % ts;
            idx.insert(idx.end(), {b[i], b[j], t[j], b[i], t[j], t[i]});
        }
    }

    // Skirts: outer drooped ring -> inner ring near the trunk, two triangles per
    // segment. Radius jitter per outer vertex gives the ragged needle edge.
    for (int k = 0; k < skirts; k++) {
        // Lowest skirt at 0.24 * height: a 7-9 m spruce keeps its needles above
        // eye level, so walking through a stand doesn't fill the screen with
        // canopy (and firing lanes exist between the trunks, DayZ-style).
        float t  = (float)k / (skirts - 1);
        float yb = 0.24f + 0.58f * t;
        float r  = (0.33f - 0.25f * t) + 0.03f;
        unsigned outer[16], inner[16];
        for (int i = 0; i < seg; i++) {
            float a  = ((float)i / seg + k * 0.37f) * 6.2831853f;
            glm::vec3 d(cosf(a), 0.0f, sinf(a));
            float jr = 1.0f + (ghash((float)(i + k * 31), 7.0f) - 0.5f) * (low ? 0.1f : 0.34f);
            glm::vec3 n  = glm::normalize(d * 0.6f + glm::vec3(0, 0.8f, 0));
            float shade  = 0.75f + 0.5f * ghash((float)i, (float)(k + 11));
            outer[i] = pushV(v, d * (r * jr) + glm::vec3(0, yb - 0.045f, 0), n,
                             needleLite * shade, low ? 0.08f : 0.13f);
            inner[i] = pushV(v, d * (r * 0.18f) + glm::vec3(0, yb + 0.10f, 0),
                             glm::vec3(0, 1, 0), needleDark, 0.03f);
        }
        for (int i = 0; i < seg; i++) {
            int j = (i + 1) % seg;
            idx.insert(idx.end(), {outer[i], outer[j], inner[j],
                                   outer[i], inner[j], inner[i]});
        }
    }

    // Top spike.
    {
        unsigned apex = pushV(v, {0, 1.0f, 0}, {0, 1, 0}, needleDark, 0.18f);
        unsigned ring[16];
        for (int i = 0; i < seg; i++) {
            float a = (float)i / seg * 6.2831853f;
            glm::vec3 d(cosf(a), 0.0f, sinf(a));
            ring[i] = pushV(v, d * 0.085f + glm::vec3(0, 0.80f, 0),
                            glm::normalize(d + glm::vec3(0, 0.7f, 0)),
                            needleLite * 0.9f, 0.10f);
        }
        for (int i = 0; i < seg; i++)
            idx.insert(idx.end(), {ring[i], ring[(i + 1) % seg], apex});
    }
}

// VAO wiring shared by every mesh-vegetation draw: 10-float vertices plus an
// 8-float-per-instance stream (two vec4 attribs, divisor 1).
GLuint vegMakeVAO(GLuint vbo, GLuint ebo, GLuint inst) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLsizei stride = 10 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
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
