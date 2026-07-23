#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "gl_loader.h"
#include "mesh.h"
#include "shader.h"

struct GrassRenderer {
    struct Instance { glm::vec4 posScale; };
    Shader shader;
    Mesh mesh;
    GLuint instanceVbo = 0;
    std::vector<Instance> activeInstances, pendingInstances;
    glm::mat4 view{1.0f}, proj{1.0f};
    int patchX=0x7fffffff, patchZ=0x7fffffff, patchMap=-1;
    int buildX=0, buildZ=0, buildRow=0;
    bool building=false, ready=false;
    GLsizei instanceCount=0;

    bool init(const char* shaderBase);
    void setCamera(const glm::mat4& v,const glm::mat4& p){view=v;proj=p;}
    void draw(const glm::vec3& eye,float time,const glm::vec3& sunDir,
              const glm::vec3& skyZenith,const glm::vec3& skyHorizon,
              const glm::vec3& groundAmbient,const glm::vec3& sunColor,
              float fogDist,float exposure,float saturation);
    void destroy();

private:
    void updatePatch(const glm::vec3& eye);
    void appendRow(int row);
    void uploadActive();
};
