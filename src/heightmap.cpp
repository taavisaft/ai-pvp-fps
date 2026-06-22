#include "heightmap.h"
#include "terrain.h"
#define STB_IMAGE_IMPLEMENTATION   // the one stb_image implementation for both exes
#include "stb_image.h"
#include <vector>
#include <cstdio>

// Height field in metres (W*H), owned here; terrain.h's gHeightData points at it.
static std::vector<float> g_store;

bool loadHeightmap(const char* primaryPath, float amp, float worldHalf) {
    const char* candidates[] = {
        primaryPath,
        "maps/field/height.png",        // run from repo root (server)
        "build/maps/field/height.png",  // run from repo root, copied tree
    };
    unsigned short* px = nullptr;
    int w = 0, h = 0, n = 0;
    const char* used = nullptr;
    for (const char* p : candidates) {
        if (!p || !*p) continue;
        px = stbi_load_16(p, &w, &h, &n, 1);   // force single 16-bit channel
        if (px) { used = p; break; }
    }
    if (!px) {
        fprintf(stderr, "heightmap: no file found (tried %s) — procedural fallback\n",
                primaryPath ? primaryPath : "(null)");
        return false;
    }

    // Normalize the used value range to 0..amp so the relief fills the chosen height,
    // regardless of how much of the 16-bit range the source image actually spans.
    size_t cnt = (size_t)w * h;
    unsigned short mn = 65535, mx = 0;
    for (size_t i = 0; i < cnt; i++) { if (px[i] < mn) mn = px[i]; if (px[i] > mx) mx = px[i]; }
    float range = (mx > mn) ? (float)(mx - mn) : 1.0f;
    g_store.resize(cnt);
    for (size_t i = 0; i < cnt; i++) g_store[i] = (float)(px[i] - mn) / range * amp;
    stbi_image_free(px);

    gHeightData   = g_store.data();
    gHeightW      = w;
    gHeightH      = h;
    gHeightHalf   = worldHalf;
    gHeightLoaded = true;
    printf("heightmap: loaded %s %dx%d amp=%.1fm\n", used, w, h, amp);
    return true;
}

void freeHeightmap() {
    g_store.clear();
    gHeightData = nullptr;
    gHeightLoaded = false;
}
