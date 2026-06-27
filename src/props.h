#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "gl_loader.h"
#include "shader.h"
#include "frustum.h"
#include "spatial.h"

struct Renderer;  // borrows lighting palette + shadow map; full type in props.cpp

// Instanced procedural-box street props for MAP_CITY: dumpsters, cars, crates, lamps.
// Each type's geometry is built from cubes in code (no asset load) into one combined
// VBO/EBO, then drawn for every instance via glDrawElementsInstanced. Instances are
// scattered deterministically around the generated city buildings (client-only, no
// collision, no net sync), bucketed into a SpatialGrid, and frustum-culled per frame.
//
// Per-instance data is 5 floats: base pos.xyz, yaw (radians), uniform scale.
enum PropType { PROP_DUMPSTER = 0, PROP_CAR, PROP_CRATE, PROP_LAMP, PROP_COUNT };

struct PropInstance { glm::vec3 pos; float yaw; float scale; };

struct PropMesh {
    GLuint  vao = 0, vbo = 0, ebo = 0, instVBO = 0;
    GLsizei indexCount = 0;
    GLint   instCap    = 0;          // GPU instance buffer capacity
    float   cullRadius = 1.0f;       // bounding sphere radius at scale 1
    float   cullCY     = 1.0f;       // sphere center height (y) at scale 1
    float   maxDist    = 200.0f;     // LOD: instances past this (from eye) are dropped
    std::vector<PropInstance> instances;
    SpatialGrid grid;                // broad-phase cells over the city
};

struct Props {
    Shader shader;        // lit instanced (per-vertex color + shadow + fog)
    Shader depthShader;   // instanced shadow-caster (depth only)
    PropMesh meshes[PROP_COUNT];
    std::vector<float> packed;   // visible-subset upload scratch (5 floats each)
    bool loaded = false;

    bool init();          // compile shaders + build the four box meshes
    void generate();      // scatter instances around gCityBuildings (current city)
    void clear();         // drop all instances (leaving the city map)
    bool empty() const;
    void draw(const Renderer& r, const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& eye, float time);
    void drawDepth(const glm::mat4& lightSpace, const glm::vec3& focus);  // shadow pass
    void destroy();

  private:
    // Grid broad-phase + per-instance sphere + distance cull -> upload to m.instVBO.
    int cullUpload(PropMesh& m, const Frustum& fr, const glm::vec3& eye);
};
