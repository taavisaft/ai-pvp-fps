#include "shader.h"
#include <cstdio>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>

// Startup-only file read; heap use here is fine (not in game loop).
static bool readFile(const char* path, char* out, size_t outSize) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "shader: cannot open %s\n", path);
        return false;
    }
    size_t n = fread(out, 1, outSize - 1, f);
    fclose(f);
    out[n] = '\0';
    return n > 0;
}

static GLuint compileStage(GLenum type, const char* src, const char* label) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        fprintf(stderr, "shader compile error (%s):\n%s\n", label, log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

bool Shader::load(const char* vertPath, const char* fragPath) {
    char vsrc[4096], fsrc[4096];
    if (!readFile(vertPath, vsrc, sizeof(vsrc))) return false;
    if (!readFile(fragPath, fsrc, sizeof(fsrc))) return false;

    GLuint vs = compileStage(GL_VERTEX_SHADER, vsrc, vertPath);
    if (!vs) return false;
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fsrc, fragPath);
    if (!fs) { glDeleteShader(vs); return false; }

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        fprintf(stderr, "shader link error:\n%s\n", log);
        glDeleteProgram(program);
        program = 0;
        return false;
    }

    locModel = glGetUniformLocation(program, "model");
    locView  = glGetUniformLocation(program, "view");
    locProj  = glGetUniformLocation(program, "proj");
    locColor = glGetUniformLocation(program, "color");
    locAlpha = glGetUniformLocation(program, "alpha");
    return true;
}

void Shader::use() const { glUseProgram(program); }

void Shader::setMat4(GLint loc, const glm::mat4& m) const {
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::setVec3(GLint loc, const glm::vec3& v) const {
    glUniform3f(loc, v.x, v.y, v.z);
}

void Shader::setFloat(GLint loc, float f) const {
    glUniform1f(loc, f);
}

void Shader::destroy() {
    if (program) { glDeleteProgram(program); program = 0; }
}
