#pragma once
#include "mesh.h"
#include "shader.h"
#include "uzi_effects.h"
struct Renderer;

struct ImportedUzi {
    Mesh body[2],bolt,casing;
    Shader shader;
    GLuint textures[3]{};
    bool init(const char* base);
    void draw(Renderer& r,const glm::mat4& mount,float boltOffset,bool low=false,bool overlay=false);
    void drawCasings(Renderer& r,const UziEffects& effects);
    void destroy();
};

// Offline rollout and matched performance comparison switch.
bool useImportedUzi();
