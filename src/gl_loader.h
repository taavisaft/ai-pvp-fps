#pragma once

// OpenGL 3.3 core function access.
// macOS: the OpenGL framework exposes core profile functions directly.
// Linux/Windows: only GL 1.1 is linkable; load the rest via SDL_GL_GetProcAddress.

#ifdef __APPLE__
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>
  inline bool loadGLFunctions() { return true; }
#else
  #include <SDL_opengl.h>

  #define GL_CORE_FUNCS(X) \
      X(PFNGLCREATESHADERPROC,            glCreateShader) \
      X(PFNGLSHADERSOURCEPROC,            glShaderSource) \
      X(PFNGLCOMPILESHADERPROC,           glCompileShader) \
      X(PFNGLGETSHADERIVPROC,             glGetShaderiv) \
      X(PFNGLGETSHADERINFOLOGPROC,        glGetShaderInfoLog) \
      X(PFNGLDELETESHADERPROC,            glDeleteShader) \
      X(PFNGLCREATEPROGRAMPROC,           glCreateProgram) \
      X(PFNGLATTACHSHADERPROC,            glAttachShader) \
      X(PFNGLLINKPROGRAMPROC,             glLinkProgram) \
      X(PFNGLGETPROGRAMIVPROC,            glGetProgramiv) \
      X(PFNGLGETPROGRAMINFOLOGPROC,       glGetProgramInfoLog) \
      X(PFNGLDELETEPROGRAMPROC,           glDeleteProgram) \
      X(PFNGLUSEPROGRAMPROC,              glUseProgram) \
      X(PFNGLGETUNIFORMLOCATIONPROC,      glGetUniformLocation) \
      X(PFNGLUNIFORMMATRIX4FVPROC,        glUniformMatrix4fv) \
      X(PFNGLUNIFORM1FPROC,               glUniform1f) \
      X(PFNGLUNIFORM3FPROC,               glUniform3f) \
      X(PFNGLUNIFORM1FPROC,               glUniform1f) \
      X(PFNGLUNIFORM1IPROC,               glUniform1i) \
      X(PFNGLACTIVETEXTUREPROC,           glActiveTexture) \
      X(PFNGLBUFFERSUBDATAPROC,           glBufferSubData) \
      X(PFNGLGENVERTEXARRAYSPROC,         glGenVertexArrays) \
      X(PFNGLBINDVERTEXARRAYPROC,         glBindVertexArray) \
      X(PFNGLDELETEVERTEXARRAYSPROC,      glDeleteVertexArrays) \
      X(PFNGLGENBUFFERSPROC,              glGenBuffers) \
      X(PFNGLBINDBUFFERPROC,              glBindBuffer) \
      X(PFNGLBUFFERDATAPROC,              glBufferData) \
      X(PFNGLDELETEBUFFERSPROC,           glDeleteBuffers) \
      X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
      X(PFNGLVERTEXATTRIBPOINTERPROC,     glVertexAttribPointer)

  #define GL_DECLARE(type, name) extern type name;
  GL_CORE_FUNCS(GL_DECLARE)
  #undef GL_DECLARE

  bool loadGLFunctions();
#endif
