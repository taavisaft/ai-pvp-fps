#pragma once
#include "mesh.h"
#include "frustum.h"
#include "khrushchevka.h"
struct Renderer;

struct KhrushVisual {
    // Eight culled sections, two material batches, near/far geometry.
    Mesh meshes[KHRUSH_SECTIONS][2][2];
    GLuint concreteTexture=0;
    bool init(const char* base);
    void draw(Renderer& r,const Frustum& fr,const glm::vec3& eye) const;
    void destroy();
};
