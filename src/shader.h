#pragma once
#include <glm/glm.hpp>
#include "gl_loader.h"

struct Shader {
    GLuint program  = 0;
    GLint  locModel = -1;
    GLint  locView  = -1;
    GLint  locProj  = -1;
    GLint  locColor = -1;
    GLint  locAlpha = -1;
    GLint  locLit   = -1;
    GLint  locEye   = -1;
    GLint  locDiffuse = -1;
    GLint  locUseTex  = -1;
    GLint  locTile    = -1;
    GLint  locSpec    = -1;
    GLint  locTint    = -1;

    bool load(const char* vertPath, const char* fragPath);
    void use() const;
    void setMat4(GLint loc, const glm::mat4& m) const;
    void setVec3(GLint loc, const glm::vec3& v) const;
    void setFloat(GLint loc, float f) const;
    void setInt(GLint loc, int v) const;
    void destroy();
};
