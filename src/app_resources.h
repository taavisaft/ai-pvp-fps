#pragma once
#include <SDL.h>
#include <cstdio>
#include <cstring>
#ifdef __APPLE__
#include <unistd.h>
#endif

// SDL resolves Contents/Resources for app bundles. Preserve CLI working paths.
inline bool prepareAppResources() {
#ifdef __APPLE__
    char* base = SDL_GetBasePath();
    if (!base) {
        std::fprintf(stderr, "Resource path: %s\n", SDL_GetError());
        return false;
    }
    bool ok = true;
    if (std::strstr(base, ".app/Contents/Resources/")) {
        ok = chdir(base) == 0;
        if (!ok) std::perror("App resources");
    }
    SDL_free(base);
    return ok;
#else
    return true;
#endif
}
