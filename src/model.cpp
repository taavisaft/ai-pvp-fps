#include "model.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <cstdio>
#include <cfloat>
#include <glm/common.hpp>

bool Model::loadGLB(const char* path) {
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    if (cgltf_parse_file(&opt, path, &d) != cgltf_result_success) return false;
    if (cgltf_load_buffers(&opt, d, path) != cgltf_result_success) {
        cgltf_free(d);
        return false;
    }

    std::vector<float>    verts;   // xyz per vertex
    std::vector<unsigned> idx;
    glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);

    for (size_t mi = 0; mi < d->meshes_count; mi++) {
        cgltf_mesh& mesh = d->meshes[mi];
        for (size_t pi = 0; pi < mesh.primitives_count; pi++) {
            cgltf_primitive& prim = mesh.primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles || !prim.indices) continue;

            cgltf_accessor* pos = nullptr;
            for (size_t a = 0; a < prim.attributes_count; a++)
                if (prim.attributes[a].type == cgltf_attribute_type_position)
                    pos = prim.attributes[a].data;
            if (!pos) continue;

            unsigned base = (unsigned)(verts.size() / 3);
            for (size_t v = 0; v < pos->count; v++) {
                float p[3];
                cgltf_accessor_read_float(pos, v, p, 3);
                verts.push_back(p[0]);
                verts.push_back(p[1]);
                verts.push_back(p[2]);
                mn = glm::min(mn, glm::vec3(p[0], p[1], p[2]));
                mx = glm::max(mx, glm::vec3(p[0], p[1], p[2]));
            }

            ModelPart part;
            part.indexOffset = (GLuint)idx.size();
            part.indexCount  = (GLsizei)prim.indices->count;
            for (size_t k = 0; k < prim.indices->count; k++)
                idx.push_back(base + (unsigned)cgltf_accessor_read_index(prim.indices, k));

            glm::vec3 col(0.8f);
            if (prim.material && prim.material->has_pbr_metallic_roughness) {
                const float* bc = prim.material->pbr_metallic_roughness.base_color_factor;
                // baseColor is linear; the renderer works in display space, so sRGB-encode
                // it — otherwise the dark metal (~0.03 linear) reads as near-black on screen.
                col = glm::pow(glm::vec3(bc[0], bc[1], bc[2]), glm::vec3(1.0f / 2.2f));
            }
            part.color = col;
            parts.push_back(part);
        }
    }
    cgltf_free(d);

    if (verts.empty() || idx.empty()) return false;
    boundsMin = mn;
    boundsMax = mx;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    printf("model: %s parts=%zu verts=%zu tris=%zu\n",
           path, parts.size(), verts.size() / 3, idx.size() / 3);
    return true;
}

void Model::destroy() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = 0;
    parts.clear();
}
