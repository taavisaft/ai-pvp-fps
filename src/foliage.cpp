#include "foliage.h"
#include "renderer.h"     // full Renderer type (lighting palette + shadow map)
#include "map.h"          // FIELD_HALF + terrain.h (terrainElevation)
#include "frustum.h"      // shared view-frustum cull
#define CGLTF_IMPLEMENTATION   // model.cpp (the other cgltf user) is not compiled
#include "cgltf.h"
#include "stb_image.h"    // implementation lives in texture.cpp
#include "texture.h"
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
    locRootBlend  = glGetUniformLocation(shader.program, "rootBlend");
    locGroundTint = glGetUniformLocation(shader.program, "groundTint");

    char dvpath[600], dfpath[600];
    snprintf(dvpath, sizeof(dvpath), "%sshaders/foliage_depth.vert", base);
    snprintf(dfpath, sizeof(dfpath), "%sshaders/foliage_depth.frag", base);
    if (!depthShader.load(dvpath, dfpath)) {
        if (!depthShader.load("shaders/foliage_depth.vert", "shaders/foliage_depth.frag"))
            return false;
    }
    locCutoffDepth = glGetUniformLocation(depthShader.program, "alphaCutoff");

    char bvpath[600], bfpath[600];
    snprintf(bvpath, sizeof(bvpath), "%sshaders/blob.vert", base);
    snprintf(bfpath, sizeof(bfpath), "%sshaders/blob.frag", base);
    if (!blobShader.load(bvpath, bfpath)) {
        if (!blobShader.load("shaders/blob.vert", "shaders/blob.frag")) return false;
    }
    locBlobRadius = glGetUniformLocation(blobShader.program, "radius");
    locBlobGround = glGetUniformLocation(blobShader.program, "groundOffset");

    // Performance-first vegetation: three alpha-tested cards at 60-degree intervals
    // (6 triangles). This reads rounder than a two-plane X while remaining tiny.
    // the coastal pine tile from the shared 4x4 vegetation atlas. The previous glTF
    // tree cost ~6k triangles per instance, which was unsuitable for a 6000-tree map.
    // Vertex layout remains pos/normal/uv so the existing instancing/shaders work.
    std::vector<float> v;
    std::vector<unsigned> idx;
    const float hw = 3.0f, h = 8.0f;
    // Atlas tile (0,3), inset to keep neighbouring tiles out of mip levels.
    const float u0 = 0.016f, u1 = 0.984f, v0 = 0.016f, v1 = 0.984f;
    auto card = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n) {
        unsigned baseIdx = (unsigned)(v.size() / 8);
        glm::vec3 p[4] = {a,b,c,d};
        glm::vec2 uv[4] = {{u0,v0},{u1,v0},{u1,v1},{u0,v1}};
        for (int i = 0; i < 4; i++)
            v.insert(v.end(), {p[i].x,p[i].y,p[i].z,n.x,n.y,n.z,uv[i].x,uv[i].y});
        idx.insert(idx.end(), {baseIdx,baseIdx+1,baseIdx+2,baseIdx,baseIdx+2,baseIdx+3});
    };
    for (int k = 0; k < 3; k++) {
        float a = (float)k * 1.04719755f;  // 0, 60, 120 degrees
        glm::vec3 axis(cosf(a) * hw, 0.0f, sinf(a) * hw);
        glm::vec3 n(-sinf(a), 0.0f, cosf(a));
        card(-axis, axis, axis + glm::vec3(0,h,0), -axis + glm::vec3(0,h,0), n);
    }
    char atlasPath[600];
    snprintf(atlasPath, sizeof(atlasPath), "%stextures/environment/baltic_vegetation_atlas_1024.png", base);
    GLuint atlas = loadTextureRGBA(atlasPath);
    if (!atlas) atlas = loadTextureRGBA("textures/environment/baltic_vegetation_atlas_1024.png");
    if (!atlas) {
        fprintf(stderr, "foliage: vegetation atlas missing — trees disabled\n");
        loaded = false;
        return true;
    }
    TreePart part;
    part.tex = atlas;
    part.indexOffset = 0;
    part.indexCount = (GLsizei)idx.size();
    part.alphaCutoff = 0.32f;
    part.doubleSided = true;
    parts.push_back(part);
    texes.push_back(atlas);
    snprintf(atlasPath, sizeof(atlasPath), "%stextures/environment/close_grass_atlas_1024.png", base);
    grassTex = loadTextureRGBA(atlasPath);
    if (!grassTex) grassTex = loadTextureRGBA("textures/environment/close_grass_atlas_1024.png");
    if (!grassTex) fprintf(stderr, "foliage: close grass atlas missing; using vegetation fallback\n");
    treeHeight = h;
    treeRadius = hw;
    printf("foliage: three-card atlas vegetation verts=%zu tris=%zu height=%.1fm\n",
           v.size() / 8, idx.size() / 3, treeHeight);

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
    const GLsizei is = 6 * sizeof(float);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, is, (void*)0);
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, is, (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, is, (void*)(5 * sizeof(float)));
    glVertexAttribDivisor(5, 1);
    glBindVertexArray(0);

    // Blob-decal quad: a unit XZ quad (corner coords -1..1) instanced from instVBO.
    static const float corners[] = {-1, -1,  1, -1,  1, 1,  -1, 1};
    static const unsigned bidx[] = {0, 1, 2, 2, 3, 0};
    GLuint bebo = 0;
    glGenVertexArrays(1, &blobVao);
    glGenBuffers(1, &blobVbo);
    glGenBuffers(1, &bebo);
    glBindVertexArray(blobVao);
    glBindBuffer(GL_ARRAY_BUFFER, blobVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(bidx), bidx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);   // same per-tree transforms
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, is, (void*)0);
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, is, (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, is, (void*)(5 * sizeof(float)));
    glVertexAttribDivisor(5, 1);
    glBindVertexArray(0);

    loaded = true;
    return true;
}

void Foliage::generate(int maxTrees) {
    if (!loaded) return;
    trees.clear();
    trees.reserve(maxTrees);
    unsigned rng = 0x9e3779b9u;   // deterministic: every client scatters the same forest
    auto rnd = [&]() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float)(rng & 0xffffff) / (float)0x1000000;
    };
    if (gMapId == MAP_LOBBY) {
        // One instance of every atlas tile, arranged in two readable rows.
        for (int i = 0; i < 16; i++) {
            glm::vec2 q(-42.0f + (i % 8) * 12.0f, i < 8 ? -34.0f : 36.0f);
            TreeInstance t;
            t.tile = (float)i;
            if (i <= 5 || i == 14 || i == 15) t.scale = 0.16f;      // grass/reed/fern/weed
            else if (i <= 9 || i == 13)        t.scale = 0.32f;      // shrubs
            else                               t.scale = 0.85f;      // saplings/pine
            t.yaw = rnd() * 6.2831853f;
            t.pos = {q.x, terrainHeight(q.x, q.y) - sink * t.scale, q.y};
            trees.push_back(t);
        }
        // Dense offline stress grove on the west side of the lobby. Clustered
        // placement creates realistic layers and deliberately exercises alpha-test
        // overdraw, while x < -18 keeps the shooting lane and target wall clear.
        constexpr glm::vec2 centers[] = {{-43,-20},{-39,2},{-45,23},{-27,25}};
        constexpr int STRESS_COUNT = 900;
        for (int i = 0; i < STRESS_COUNT; i++) {
            const glm::vec2 c = centers[i % 4];
            float angle = rnd() * 6.2831853f;
            float radius = sqrtf(rnd()) * (7.0f + 5.0f * rnd());
            glm::vec2 q = c + glm::vec2(cosf(angle), sinf(angle)) * radius;
            q.x = glm::clamp(q.x, -57.0f, -18.0f);
            q.y = glm::clamp(q.y, -52.0f, 52.0f);

            float pick = rnd();
            int tile;
            float scale;
            if (pick < 0.48f) {                 // grass/reeds: dense understory
                tile = (int)(rnd() * 6.0f);
                scale = 0.10f + rnd() * 0.10f;
            } else if (pick < 0.78f) {          // shrubs/dead brush
                tile = 6 + (int)(rnd() * 4.0f);
                if (rnd() < 0.12f) tile = 13;
                scale = 0.22f + rnd() * 0.20f;
            } else if (pick < 0.88f) {          // fern/coastal weeds
                tile = 14 + (int)(rnd() * 2.0f);
                scale = 0.12f + rnd() * 0.12f;
            } else {                            // sparse canopy above understory
                tile = rnd() < 0.62f ? 12 : 10 + (int)(rnd() * 2.0f);
                scale = 0.65f + rnd() * 0.50f;
            }
            TreeInstance t;
            t.tile = (float)tile;
            t.scale = scale;
            t.yaw = rnd() * 6.2831853f;
            t.pos = {q.x, terrainHeight(q.x, q.y) - sink * scale, q.y};
            trees.push_back(t);
        }
        // General lobby grass field for offline visual/performance testing. Keep the
        // central x-directed firing lane (|z| < 13) sparse and leave breathing room
        // around the player spawn, target wall, and test-cover cluster.
        constexpr int LOBBY_GRASS = 2600;
        int made = 0, attempts = 0;
        while (made < LOBBY_GRASS && attempts++ < LOBBY_GRASS * 5) {
            float x = (rnd() * 2.0f - 1.0f) * 56.0f;
            float z = (rnd() * 2.0f - 1.0f) * 56.0f;
            if (fabsf(z) < 13.0f && x > -14.0f) continue;        // firing lane
            float sx = x - 12.0f, sz = z;                        // spawn clearance
            if (sx*sx + sz*sz < 9.0f * 9.0f) continue;
            if (x > 22.0f && fabsf(z) < 17.0f) continue;         // target-wall apron
            bool nearBox = false;
            for (int bi = 0; bi < gMapBoxCount; bi++) {
                const Box& b = gMapBoxes[bi];
                if (fabsf(x - b.center.x) < b.half.x + 1.2f &&
                    fabsf(z - b.center.z) < b.half.z + 1.2f) { nearBox = true; break; }
            }
            if (nearBox) continue;
            float choose = rnd();
            int tile = choose < 0.72f ? (int)(rnd() * 4.0f)
                     : choose < 0.88f ? 4 + (int)(rnd() * 2.0f)
                     : 14 + (int)(rnd() * 2.0f);
            TreeInstance g;
            g.tile = (float)tile;
            g.scale = 0.09f + rnd() * 0.10f;
            g.yaw = rnd() * 6.2831853f;
            g.pos = {x, terrainHeight(x, z) - sink * g.scale, z};
            trees.push_back(g);
            made++;
        }
        grid.init(LOBBY_HALF, 8, -3.0f, 18.0f);
        for (int i = 0; i < (int)trees.size(); i++)
            grid.insert(trees[i].pos.x, trees[i].pos.z, i);
        glBindBuffer(GL_ARRAY_BUFFER, instVBO);
        glBufferData(GL_ARRAY_BUFFER, trees.size() * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        instCap = (GLint)trees.size();
        printf("foliage: lobby has %zu showcase/stress/grass instances\n", trees.size());
        return;
    }
    auto sstep = [](float a, float b, float x) {
        float t = (x - a) / (b - a);
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * (3.0f - 2.0f * t);
    };
    // Forest mask: low-freq noise carves big wooded patches with open clearings
    // between, so trees clump into forests "now and then" instead of even scatter.
    auto forestDensity = [&](float x, float z) {
        float f = terrValueNoise(x * 0.045f + 50.0f, z * 0.045f + 50.0f);   // ~22 m woods
        f = f * 0.65f + terrValueNoise(x * 0.13f, z * 0.13f) * 0.35f;       // ragged edges
        return sstep(0.44f, 0.60f, f);   // 0 = clearing, 1 = dense forest
    };
    const float SPAN = PALDISKI_HALF * 0.96f;
    const float e = 2.0f;
    const float SINK = sink;     // bury the trunk base so trees never look floaty
    int attempts = 0, maxAttempts = maxTrees * 60;
    while ((int)trees.size() < maxTrees && attempts < maxAttempts) {
        attempts++;
        float x = (rnd() * 2.0f - 1.0f) * SPAN;
        float z = (rnd() * 2.0f - 1.0f) * SPAN;
        if (rnd() > forestDensity(x, z)) continue;               // keep to wooded patches
        float h = terrainHeight(x, z);
        if (h < 1.5f) continue;                                  // no trees in sea/beach
        bool onPad = false;                                      // keep town yards clear
        for (int i = 0; i < gTerrainPadCount; i++) {
            float dx = x - gTerrainPads[i].x, dz = z - gTerrainPads[i].z;
            if (dx * dx + dz * dz < gTerrainPads[i].r * gTerrainPads[i].r) { onPad = true; break; }
        }
        if (onPad) continue;
        float dhdx = (terrainHeight(x + e, z) - terrainHeight(x - e, z)) / (2 * e);
        float dhdz = (terrainHeight(x, z + e) - terrainHeight(x, z - e)) / (2 * e);
        if (sqrtf(dhdx * dhdx + dhdz * dhdz) > 0.55f) continue;  // skip steep ground (rock)
        TreeInstance ti;
        ti.yaw   = rnd() * 6.2831853f;
        ti.scale = 0.85f + rnd() * 0.95f;   // bigger, more varied canopies
        ti.tile  = rnd() < 0.28f ? (10.0f + floorf(rnd() * 2.0f)) : 12.0f;
        // The atlas sprite has transparent space below its visible roots. Scale the
        // burial with the card or large trees regain that gap and appear to float.
        ti.pos   = glm::vec3(x, h - SINK * ti.scale, z);
        trees.push_back(ti);
    }
    // Broad grass biome: clumps occupy most valid land, but only clumps within 60 m
    // are uploaded. This gives dense local coverage without drawing the full map.
    constexpr int GRASS_COUNT = 35000;
    int grassMade = 0, grassAttempts = 0;
    while (grassMade < GRASS_COUNT && grassAttempts++ < GRASS_COUNT * 4) {
        float x = (rnd() * 2.0f - 1.0f) * SPAN;
        float z = (rnd() * 2.0f - 1.0f) * SPAN;
        float h = terrainHeight(x, z);
        if (h < 0.35f) continue;                                  // sea/wet beach
        bool onPad = false;
        for (int i = 0; i < gTerrainPadCount; i++) {
            float dx = x - gTerrainPads[i].x, dz = z - gTerrainPads[i].z;
            float safeR = gTerrainPads[i].r * 0.72f;
            if (dx*dx + dz*dz < safeR*safeR) { onPad = true; break; }
        }
        if (onPad) continue;
        float dhx = (terrainHeight(x + e, z) - terrainHeight(x - e, z)) / (2 * e);
        float dhz = (terrainHeight(x, z + e) - terrainHeight(x, z - e)) / (2 * e);
        if (sqrtf(dhx*dhx + dhz*dhz) > 0.65f) continue;
        float r = rnd();
        int tile = r < 0.72f ? (int)(rnd() * 4.0f)
                 : r < 0.88f ? 4 + (int)(rnd() * 2.0f)
                 : 14 + (int)(rnd() * 2.0f);
        TreeInstance g;
        g.tile = (float)tile;
        g.scale = 0.09f + rnd() * 0.10f;
        g.yaw = rnd() * 6.2831853f;
        g.pos = {x, h - sink * g.scale, z};
        trees.push_back(g);
        grassMade++;
    }
    grid.init(PALDISKI_HALF, 24, -12.0f, 45.0f);
    for (int i = 0; i < (int)trees.size(); i++)
        grid.insert(trees[i].pos.x, trees[i].pos.z, i);
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    glBufferData(GL_ARRAY_BUFFER, trees.size() * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    instCap = (GLint)trees.size();
    printf("foliage: %d trees + %d grass clumps (%zu total instances)\n",
           maxTrees, grassMade, trees.size());
}

void Foliage::clear() { trees.clear(); grid.clear(); }

int Foliage::cullUpload(const glm::mat4& vp, const glm::vec3* eye, int filter) {
    Frustum fr = Frustum::fromVP(vp);

    packed.clear();
    float cullR = glm::max(treeHeight * 0.5f, treeRadius);
    auto consider = [&](int index) {
        const TreeInstance& t = trees[index];
        bool grass = t.tile <= 5.0f || t.tile >= 14.0f;
        bool small = t.tile < 10.0f || t.tile >= 13.0f;
        if (filter == 1 && grass) return;
        if (filter == 2 && !grass) return;
        glm::vec3 c = t.pos + glm::vec3(0.0f, treeHeight * 0.5f * t.scale, 0.0f);
        float scale = t.scale;
        if (small && eye) {
            // Small clutter fades out by shrinking into the ground instead of the
            // old hard 60 m pop; the ground texture carries the look beyond.
            float dist = glm::length(c - *eye);
            float fade = 1.0f - glm::smoothstep(42.0f, 60.0f, dist);
            if (fade < 0.03f) return;
            scale *= fade;
        }
        if (!fr.sphereVisible(c, cullR * t.scale)) return;
        packed.insert(packed.end(), {t.pos.x, t.pos.y, t.pos.z, t.yaw, scale, t.tile});
    };
    if (!grid.empty()) grid.forEachVisible(fr, consider);
    else for (int i = 0; i < (int)trees.size(); i++) consider(i);
    int visible = (int)(packed.size() / 6);
    if (visible == 0) return 0;
    glBindBuffer(GL_ARRAY_BUFFER, instVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, packed.size() * sizeof(float), packed.data());
    return visible;
}

void Foliage::draw(const Renderer& r, const glm::mat4& view, const glm::mat4& proj,
                   const glm::vec3& eye, float time) {
    if (!loaded || trees.empty()) return;
    int visible = cullUpload(proj * view, &eye, 1);

    shader.use();
    shader.setMat4(shader.locView, view);
    shader.setMat4(shader.locProj, proj);
    shader.setVec3(shader.locEye, eye);
    shader.setFloat(shader.locTime, time);
    shader.setVec3(shader.locSunDir, r.sunDir);
    shader.setVec3(shader.locSkyZenith, r.skyZenith);
    shader.setVec3(shader.locSkyHorizon, r.skyHorizon);
    shader.setVec3(shader.locGroundAmb, r.groundAmbient);
    shader.setVec3(shader.locSunColor, r.sunColor);
    shader.setFloat(shader.locFogDist, r.fogDist);
    shader.setFloat(shader.locFogHeight, r.fogHeightAmt);
    shader.setFloat(shader.locCloud, r.cloudAmount);
    shader.setFloat(shader.locExposure, r.exposure);
    shader.setFloat(shader.locSaturation, r.saturation);
    shader.setMat4(shader.locLightSpace, r.lightSpace);
    shader.setInt(shader.locShadowMap, 1);
    shader.setInt(shader.locUseShadow, 1);
    shader.setInt(shader.locDiffuse, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, r.shadowTex);

    // Ground albedo the card roots blend into (matches textures/ground.jpg mean).
    const glm::vec3 groundTint(0.58f, 0.53f, 0.35f);
    shader.setVec3(locGroundTint, groundTint);
    shader.setFloat(locRootBlend, 0.35f);   // trees/shrubs: mild grounding only

    glBindVertexArray(vao);
    if (visible > 0)
        for (const TreePart& part : parts) {
            if (part.doubleSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
            shader.setFloat(locCutoff, part.alphaCutoff);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, part.tex);
            glDrawElementsInstanced(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT,
                                    (void*)(size_t)part.indexOffset, visible);
        }

    // Far blob decals for woody vegetation only (the instance buffer still holds the
    // filter-1 set); grass never receives one. The shader fades them in by distance
    // so near trees keep their real dappled shadow and only distant ones get the blob.
    if (visible > 0) {
        blobShader.use();
        blobShader.setMat4(blobShader.locView, view);
        blobShader.setMat4(blobShader.locProj, proj);
        blobShader.setVec3(blobShader.locEye, eye);
        blobShader.setFloat(locBlobRadius, treeRadius);
        blobShader.setFloat(locBlobGround, sink + 0.06f);   // sit above sunk base
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);          // decal: test depth, don't write it
        glDisable(GL_CULL_FACE);
        glBindVertexArray(blobVao);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, visible);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    // Grass is a second instanced batch using the dedicated dense-patch atlas,
    // uploaded last so it overwrites the shared instance buffer after the blobs.
    int grassVisible = cullUpload(proj * view, &eye, 2);
    if (grassVisible > 0) {
        shader.use();
        glBindVertexArray(vao);
        glDisable(GL_CULL_FACE);
        shader.setFloat(locCutoff, 0.32f);
        shader.setFloat(locRootBlend, 1.0f);   // grass roots merge fully with ground
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTex ? grassTex : parts[0].tex);
        glDrawElementsInstanced(GL_TRIANGLES, parts[0].indexCount, GL_UNSIGNED_INT,
                                (void*)(size_t)parts[0].indexOffset, grassVisible);
    }
    glEnable(GL_CULL_FACE);   // restore world default
    glBindVertexArray(0);
}

void Foliage::drawDepth(const glm::mat4& lightSpace, float time) {
    if (!loaded || trees.empty()) return;
    int visible = cullUpload(lightSpace, nullptr, 1); // grass does not cast shadows
    if (visible == 0) return;

    depthShader.use();
    depthShader.setMat4(depthShader.locLightSpace, lightSpace);
    depthShader.setFloat(depthShader.locTime, time);   // sway must match the lit pass
    depthShader.setInt(depthShader.locDiffuse, 0);

    // Shadow pass already disabled face culling (beginShadowPass), so leaves cast
    // from both sides. Just write depth, alpha-cutting leaf gaps.
    glBindVertexArray(vao);
    for (const TreePart& part : parts) {
        depthShader.setFloat(locCutoffDepth, part.alphaCutoff);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, part.tex);
        glDrawElementsInstanced(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT,
                                (void*)(size_t)part.indexOffset, visible);
    }
    glBindVertexArray(0);
}

void Foliage::destroy() {
    for (GLuint t : texes) if (t) glDeleteTextures(1, &t);
    if (grassTex) glDeleteTextures(1, &grassTex);
    grassTex = 0;
    texes.clear();
    if (blobVbo) glDeleteBuffers(1, &blobVbo);
    if (blobVao) glDeleteVertexArrays(1, &blobVao);
    if (instVBO) glDeleteBuffers(1, &instVBO);
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = instVBO = blobVao = blobVbo = 0;
    shader.destroy();
    depthShader.destroy();
    blobShader.destroy();
    trees.clear();
    grid.clear();
    parts.clear();
}
