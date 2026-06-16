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
      X(PFNGLCREATESHADERPROC,            fps_glCreateShader,            glCreateShader) \
      X(PFNGLSHADERSOURCEPROC,            fps_glShaderSource,            glShaderSource) \
      X(PFNGLCOMPILESHADERPROC,           fps_glCompileShader,           glCompileShader) \
      X(PFNGLGETSHADERIVPROC,             fps_glGetShaderiv,             glGetShaderiv) \
      X(PFNGLGETSHADERINFOLOGPROC,        fps_glGetShaderInfoLog,        glGetShaderInfoLog) \
      X(PFNGLDELETESHADERPROC,            fps_glDeleteShader,            glDeleteShader) \
      X(PFNGLCREATEPROGRAMPROC,           fps_glCreateProgram,           glCreateProgram) \
      X(PFNGLATTACHSHADERPROC,            fps_glAttachShader,            glAttachShader) \
      X(PFNGLLINKPROGRAMPROC,             fps_glLinkProgram,             glLinkProgram) \
      X(PFNGLGETPROGRAMIVPROC,            fps_glGetProgramiv,            glGetProgramiv) \
      X(PFNGLGETPROGRAMINFOLOGPROC,       fps_glGetProgramInfoLog,       glGetProgramInfoLog) \
      X(PFNGLDELETEPROGRAMPROC,           fps_glDeleteProgram,           glDeleteProgram) \
      X(PFNGLUSEPROGRAMPROC,              fps_glUseProgram,              glUseProgram) \
      X(PFNGLGETUNIFORMLOCATIONPROC,      fps_glGetUniformLocation,      glGetUniformLocation) \
      X(PFNGLUNIFORMMATRIX4FVPROC,        fps_glUniformMatrix4fv,        glUniformMatrix4fv) \
      X(PFNGLUNIFORM1FPROC,               fps_glUniform1f,               glUniform1f) \
      X(PFNGLUNIFORM3FPROC,               fps_glUniform3f,               glUniform3f) \
      X(PFNGLUNIFORM1IPROC,               fps_glUniform1i,               glUniform1i) \
      X(PFNGLACTIVETEXTUREPROC,           fps_glActiveTexture,           glActiveTexture) \
      X(PFNGLBUFFERSUBDATAPROC,           fps_glBufferSubData,           glBufferSubData) \
      X(PFNGLGENVERTEXARRAYSPROC,         fps_glGenVertexArrays,         glGenVertexArrays) \
      X(PFNGLBINDVERTEXARRAYPROC,         fps_glBindVertexArray,         glBindVertexArray) \
      X(PFNGLDELETEVERTEXARRAYSPROC,      fps_glDeleteVertexArrays,      glDeleteVertexArrays) \
      X(PFNGLGENBUFFERSPROC,              fps_glGenBuffers,              glGenBuffers) \
      X(PFNGLBINDBUFFERPROC,              fps_glBindBuffer,              glBindBuffer) \
      X(PFNGLBUFFERDATAPROC,              fps_glBufferData,              glBufferData) \
      X(PFNGLDELETEBUFFERSPROC,           fps_glDeleteBuffers,           glDeleteBuffers) \
      X(PFNGLENABLEVERTEXATTRIBARRAYPROC, fps_glEnableVertexAttribArray, glEnableVertexAttribArray) \
      X(PFNGLVERTEXATTRIBPOINTERPROC,     fps_glVertexAttribPointer,     glVertexAttribPointer)

  #define GL_DECLARE(type, name, symbol) extern type name;
  GL_CORE_FUNCS(GL_DECLARE)
  #undef GL_DECLARE

  #define glCreateShader            fps_glCreateShader
  #define glShaderSource            fps_glShaderSource
  #define glCompileShader           fps_glCompileShader
  #define glGetShaderiv             fps_glGetShaderiv
  #define glGetShaderInfoLog        fps_glGetShaderInfoLog
  #define glDeleteShader            fps_glDeleteShader
  #define glCreateProgram           fps_glCreateProgram
  #define glAttachShader            fps_glAttachShader
  #define glLinkProgram             fps_glLinkProgram
  #define glGetProgramiv            fps_glGetProgramiv
  #define glGetProgramInfoLog       fps_glGetProgramInfoLog
  #define glDeleteProgram           fps_glDeleteProgram
  #define glUseProgram              fps_glUseProgram
  #define glGetUniformLocation      fps_glGetUniformLocation
  #define glUniformMatrix4fv        fps_glUniformMatrix4fv
  #define glUniform1f               fps_glUniform1f
  #define glUniform3f               fps_glUniform3f
  #define glUniform1i               fps_glUniform1i
  #define glActiveTexture           fps_glActiveTexture
  #define glBufferSubData           fps_glBufferSubData
  #define glGenVertexArrays         fps_glGenVertexArrays
  #define glBindVertexArray         fps_glBindVertexArray
  #define glDeleteVertexArrays      fps_glDeleteVertexArrays
  #define glGenBuffers              fps_glGenBuffers
  #define glBindBuffer              fps_glBindBuffer
  #define glBufferData              fps_glBufferData
  #define glDeleteBuffers           fps_glDeleteBuffers
  #define glEnableVertexAttribArray fps_glEnableVertexAttribArray
  #define glVertexAttribPointer     fps_glVertexAttribPointer

  bool loadGLFunctions();
#endif
