#include "foliage.h"
#include "renderer.h"     // full Renderer type (lighting palette + shadow map)
#include "map.h"          // FIELD_HALF + terrain.h (terrainElevation)
#define CGLTF_IMPLEMENTATION   // model.cpp (the other cgltf user) is not compiled
#include "cgltf.h"
#include "stb_image.h"    // implementation lives in texture.cpp
#include <cmath>
#include <cstdio>
#include <cfloat>
#include <map>
#include <glm/gtc/matrix_transform.hpp>

namespace {
constexpr float TARGET_HEIGHT = 8.0f;   // baked tree height at instance scale 1 (metres)

// Decode an embedded glTF image (PNG/JPG bytes in a buffer view) to an RGBA GL texture.
GLuint uploadEmbeddedImage(const cgltf_image* img) {
    if (!img || !img->buffer_view) return 0;
    const cgltf_buffer_view* bv = img->buffer_view;
    const unsigned char* bytes = (const unsigned char*)bv->buffer->data + bv->offset;
    int w = 0, h = 0, c = 0;
    stbi_set_flip_vertically_on_load(0);   // glTF UVs are top-left; shader flips V
    unsigned char* px = stbi_load_from_memory(bytes, (int)bv->size, &w, &h, &c, 4);
    if (!px) return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Mipmapped minify softens alpha edges; keep crisp leaves with alpha-test in frag.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(px);
    return tex;
}
}  // namespace

// Load models/tree.glb into the combined VBO/EBO + per-material parts. Vertex layout:
// pos.xyz, normal.xyz, uv.xy (8 floats). Positions are baked so the tree's feet sit at
// y=0, it is centred on XZ, and it stands TARGET_HEIGHT tall at instance scale 1.
static bool loadTreeGLB(const char* path, std::vector<float>& verts,
                        std::vector<unsigned>& idx, std::vector<TreePart>& parts,
                        std::vector<GLuint>& texes, float& outHeight, float& outRadius) {
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    if (cgltf_parse_file(&opt, path, &d) != cgltf_result_success) return false;
    if (cgltf_load_buffers(&opt, d, path) != cgltf_result_success) { cgltf_free(d); return false; }

    std::map<const cgltf_image*, GLuint> imgCache;   // dedupe shared textures
    glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);

    for (size_t mi = 0; mi < d->meshes_count; mi++) {
        cgltf_mesh& mesh = d->meshes[mi];
        for (size_t pi = 0; pi < mesh.primitives_count; pi++) {
            cgltf_primitive& prim = mesh.primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles || !prim.indices) continue;
            cgltf_accessor *pos = nullptr, *nrm = nullptr, *uv = nullptr;
            for (size_t a = 0; a < prim.attributes_count; a++) {
                cgltf_attribute& at = prim.attributes[a];
                if (at.type == cgltf_attribute_type_position) pos = at.data;
                else if (at.type == cgltf_attribute_type_normal) nrm = at.data;
                else if (at.type == cgltf_attribute_type_texcoord && !uv) uv = at.data;
            }
            if (!pos) continue;

            unsigned base = (unsigned)(verts.size() / 8);
            for (size_t v = 0; v < pos->count; v++) {
                float p[3] = {0, 0, 0}, n[3] = {0, 1, 0}, t[2] = {0, 0};
                cgltf_accessor_read_float(pos, v, p, 3);
                if (nrm) cgltf_accessor_read_float(nrm, v, n, 3);
                if (uv)  cgltf_accessor_read_float(uv, v, t, 2);
                verts.insert(verts.end(), {p[0], p[1], p[2], n[0], n[1], n[2], t[0], t[1]});
                mn = glm::min(mn, glm::vec3(p[0], p[1], p[2]));
                mx = glm::max(mx, glm::vec3(p[0], p[1], p[2]));
            }

            TreePart part;
            part.indexOffset = (GLuint)(idx.size() * sizeof(unsigned));
            part.indexCount  = (GLsizei)prim.indices->count;
            for (size_t k = 0; k < prim.indices->count; k++)
                idx.push_back(base + (unsigned)cgltf_accessor_read_index(prim.indices, k));

            if (prim.material) {
                if (prim.material->alpha_mode == cgltf_alpha_mode_mask)
                    part.alphaCutoff = prim.material->alpha_cutoff;
                part.doubleSided = prim.material->double_sided;
                if (prim.material->has_pbr_metallic_roughness) {
                    const cgltf_image* img =
                        prim.material->pbr_metallic_roughness.base_color_texture.texture
                            ? prim.material->pbr_metallic_roughness.base_color_texture.texture->image
                            : nullptr;
                    if (img) {
                        auto it = imgCache.find(img);
                        if (it == imgCache.end()) {
                            GLuint t = uploadEmbeddedImage(img);
                            imgCache[img] = t;
                            if (t) texes.push_back(t);
                            part.tex = t;
                        } else part.tex = it->second;
                    }
                }
            }
            parts.push_back(part);
        }
    }
    cgltf_free(d);
    if (verts.empty() || parts.empty()) return false;

    // Bake transform: feet to y=0, centre XZ, scale to TARGET_HEIGHT.
    float height = glm::max(mx.y - mn.y, 1e-3f);
    float s = TARGET_HEIGHT / height;
    glm::vec3 off(-(mn.x + mx.x) * 0.5f, -mn.y, -(mn.z + mx.z) * 0.5f);
    for (size_t i = 0; i < verts.size(); i += 8) {
        verts[i + 0] = (verts[i + 0] + off.x) * s;
        verts[i + 1] = (verts[i + 1] + off.y) * s;
        verts[i + 2] = (verts[i + 2] + off.z) * s;
        // normals unchanged by uniform scale + translation
    }
    outHeight = TARGET_HEIGHT;
    outRadius = glm::max(mx.x - mn.x, mx.z - mn.z) * 0.5f * s;
    printf("foliage: loaded %s parts=%zu verts=%zu tris=%zu height->%.1fm\n",
           path, parts.size(), verts.size() / 8, idx.size() / 3, TARGET_HEIGHT);
    return true;
}

bool Foliage::init() {
    char base[512];
    const char* sdlBase = SDL_GetBasePath();
    char vpath[600], fpath[600];
    snprintf(base, sizeof(base), "%s", sdlBase ? sdlBase : "");
    snprintf(vpath, sizeof(vpath), "%sshaders/foliage.vert", base);
    snprintf(fpath, sizeof(fpath), "%sshaders/foliage.frag", base);
    if (!shader.load(vpath, fpath)) {
        if (!shader.load("shaders/foliage.vert", "shaders/foliage.frag")) return false;
    }
    locCutoff = glGetUniformLocation(shader.program, "alphaCutoff");

    std::vector<float>    v;
    std::vector<unsigned> idx;
    char mpath[600];
    snprintf(mpath, sizeof(mpath), "%smodels/tree.glb", base);
    if (!loadTreeGLB(mpath, v, idx, parts, texes, treeHeight, treeRadius)) {
        if (!loadTreeGLB("models/tree.glb", v, idx, parts, texes, treeHeight, treeRadius)) {
            fprintf(stderr, "foliage: models/tree.glb not found — trees disabled\n");
            loaded = false;
            return true;   // optional system; renderer still starts
        }
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glGenBuffers(1, &instVBO);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    const GLsizei vs = 8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vs, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vs, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, vs, (void*)(6 * sizeof(float)));

    // Per-instance attributes (divisor 1): pos.xyz (loc 3), yaw+scale (loc 4).
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    const GLsizei is = 5 * sizeof(float);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, is, (void*)0);
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, is, (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);
    loaded = true;
    return true;
}

void Foliage::generate(int count) {
    if (!loaded) return;
    trees.clear();
    trees.reserve(count);
    unsigned rng = 0x9e3779b9u;   // deterministic: every client scatters the same forest
    auto rnd = [&]() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float)(rng & 0xffffff) / (float)0x1000000;
    };
    const float SPAN = FIELD_HALF * 0.96f;
    const float e = 2.0f;
    int attempts = 0;
    while ((int)trees.size() < count && attempts < count * 20) {
        attempts++;
        float x = (rnd() * 2.0f - 1.0f) * SPAN;
        float z = (rnd() * 2.0f - 1.0f) * SPAN;
        float dhdx = (terrainElevation(x + e, z) - terrainElevation(x - e, z)) / (2 * e);
        float dhdz = (terrainElevation(x, z + e) - terrainElevation(x, z - e)) / (2 * e);
        if (sqrtf(dhdx * dhdx + dhdz * dhdz) > 0.55f) continue;   // skip steep ground (rock)
        TreeInstance ti;
        ti.pos   = glm::vec3(x, terrainElevation(x, z), z);
        ti.yaw   = rnd() * 6.2831853f;
        ti.scale = 0.75f + rnd() * 0.75f;
        trees.push_back(ti);
    }
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    glBufferData(GL_ARRAY_BUFFER, trees.size() * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    instCap = (GLint)trees.size();
    printf("foliage: scattered %zu trees (%d attempts)\n", trees.size(), attempts);
}

void Foliage::clear() { trees.clear(); }

void Foliage::draw(const Renderer& r, const glm::mat4& view, const glm::mat4& proj,
                   const glm::vec3& eye, float time) {
    if (!loaded || trees.empty()) return;

    // Frustum planes from VP (Gribb-Hartmann). GLM is column-major: m[col][row].
    glm::mat4 m = proj * view;
    glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
    glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
    glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
    glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);
    glm::vec4 planes[6] = {row3 + row0, row3 - row0, row3 + row1,
                           row3 - row1, row3 + row2, row3 - row2};
    for (auto& p : planes) p /= glm::length(glm::vec3(p));

    packed.clear();
    float cullR = glm::max(treeHeight * 0.5f, treeRadius);
    for (const TreeInstance& t : trees) {
        glm::vec3 c = t.pos + glm::vec3(0.0f, treeHeight * 0.5f * t.scale, 0.0f);
        float rad = cullR * t.scale;
        bool in = true;
        for (auto& p : planes)
            if (glm::dot(glm::vec3(p), c) + p.w < -rad) { in = false; break; }
        if (!in) continue;
        packed.insert(packed.end(), {t.pos.x, t.pos.y, t.pos.z, t.yaw, t.scale});
    }
    int visible = (int)(packed.size() / 5);
    if (visible == 0) return;

    shader.use();
    shader.setMat4(shader.locView, view);
    shader.setMat4(shader.locProj, proj);
    shader.setVec3(shader.locEye, eye);
    shader.setFloat(shader.locTime, time);
    shader.setVec3(shader.locSunDir, r.sunDir);
    shader.setVec3(shader.locSkyZenith, r.skyZenith);
    shader.setVec3(shader.locSkyHorizon, r.skyHorizon);
    shader.setVec3(shader.locGroundAmb, r.groundAmbient);
    shader.setMat4(shader.locLightSpace, r.lightSpace);
    shader.setInt(shader.locShadowMap, 1);
    shader.setInt(shader.locUseShadow, 1);
    shader.setInt(shader.locDiffuse, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, r.shadowTex);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, packed.size() * sizeof(float), packed.data());

    for (const TreePart& part : parts) {
        if (part.doubleSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
        shader.setFloat(locCutoff, part.alphaCutoff);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, part.tex);
        glDrawElementsInstanced(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT,
                                (void*)(size_t)part.indexOffset, visible);
    }
    glEnable(GL_CULL_FACE);   // restore world default
    glBindVertexArray(0);
}

void Foliage::destroy() {
    for (GLuint t : texes) if (t) glDeleteTextures(1, &t);
    texes.clear();
    if (instVBO) glDeleteBuffers(1, &instVBO);
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = instVBO = 0;
    shader.destroy();
    trees.clear();
    parts.clear();
}
