#include "gl_loader.h"

#ifndef __APPLE__
#include <SDL.h>
#include <cstdio>

#define GL_DEFINE(type, name, symbol) type name = nullptr;
GL_CORE_FUNCS(GL_DEFINE)
#undef GL_DEFINE

bool loadGLFunctions() {
    bool ok = true;
    #define GL_LOAD_FUNC(type, name, symbol) \
        name = (type)SDL_GL_GetProcAddress(#symbol); \
        if (!name) { fprintf(stderr, "GL load failed: %s\n", #symbol); ok = false; }
    GL_CORE_FUNCS(GL_LOAD_FUNC)
    #undef GL_LOAD_FUNC
    return ok;
}
#endif
