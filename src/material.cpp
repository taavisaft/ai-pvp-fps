#include "material.h"
#include "texture.h"
#include <SDL.h>
#include <cstdio>
#include <string>

bool MaterialLib::init() {
    // Per material: try textures/<name>.png then .jpg (stb decodes both); fall back to
    // the procedural generator when no file is present. Loaded images carry their own
    // color, so their tint is neutralised; procedural paths keep the tints set below.
    struct Entry { MaterialId id; const char* name; GLuint (*proc)(); };
    const Entry table[] = {
        {MAT_GROUND,   "ground",   makeGroundTexture},
        {MAT_CONCRETE, "concrete", makeConcreteTexture},
        {MAT_METAL,    "metal",    makeMetalTexture},
        {MAT_WOOD,     "wood",     makeWoodTexture},
        {MAT_ROCK,     "rock",     makeRockTexture},
        {MAT_DIRT,     "dirt",     makeDirtTexture},
    };

    mats[MAT_GROUND].tint   = {1.0f, 1.0f, 1.0f};
    mats[MAT_CONCRETE].tint = {1.0f, 1.0f, 1.0f};
    mats[MAT_METAL].tint    = {0.85f, 0.88f, 0.92f};
    mats[MAT_WOOD].tint     = {1.0f, 1.0f, 1.0f};
    mats[MAT_ROCK].tint     = {1.0f, 1.0f, 1.0f};
    mats[MAT_DIRT].tint     = {1.0f, 1.0f, 1.0f};

    for (const Entry& e : table) {
        std::string base = std::string("textures/") + e.name;
        GLuint tex = loadTexture((base + ".png").c_str());
        if (!tex) tex = loadTexture((base + ".jpg").c_str());
        if (tex) {
            mats[e.id].tint = {1.0f, 1.0f, 1.0f};        // image has its own color
            if (e.id == MAT_GROUND) {
                groundHasImage = true;
                // Cool the brown forest-floor photo into the mossy underlayer visible
                // between blades. Exposed soil still reads through, but the field no
                // longer looks like grass cards scattered over beige sand.
                mats[e.id].tint = {0.72f, 0.88f, 0.60f};
            }
            printf("material: loaded image for %s\n", e.name);
        } else {
            tex = e.proc();                              // procedural fallback
        }
        mats[e.id].tex = tex;
    }

    mats[MAT_GROUND].tile   = 2.0f;
    mats[MAT_CONCRETE].tile = 1.0f;
    mats[MAT_METAL].tile    = 1.0f;
    mats[MAT_WOOD].tile     = 0.8f;
    mats[MAT_ROCK].tile     = 1.5f;
    mats[MAT_DIRT].tile     = 1.5f;

    mats[MAT_GROUND].spec   = 0.0f;
    mats[MAT_CONCRETE].spec = 0.05f;
    mats[MAT_METAL].spec    = 0.35f;
    mats[MAT_WOOD].spec     = 0.02f;
    mats[MAT_ROCK].spec     = 0.04f;
    mats[MAT_DIRT].spec     = 0.0f;

    for (int i = 0; i < MAT_COUNT; i++) {
        if (!mats[i].tex) {
            fprintf(stderr, "material: failed to create texture %d\n", i);
            destroy();
            return false;
        }
    }

    // Pine forest floor: photographic needles, moss and tiny plants replace blade
    // geometry in this biome. Prefer the packaged copy beside the executable.
    char forestPath[768];
    const char* exeBase = SDL_GetBasePath();
    snprintf(forestPath, sizeof(forestPath), "%stextures/environment/forest_ground_1k.jpg",
             exeBase ? exeBase : "");
    forestGroundTex = loadTexture(forestPath);
    if (!forestGroundTex)
        forestGroundTex = loadTexture("textures/environment/forest_ground_1k.jpg");
    if (!forestGroundTex) {
        fprintf(stderr, "material: pine forest floor missing, using ground fallback\n");
        forestGroundTex = mats[MAT_GROUND].tex;
    } else {
        printf("material: loaded pine forest floor\n");
    }
    return true;
}

void MaterialLib::bind(MaterialId id) const {
    int i = (int)id;
    if (i < 0 || i >= MAT_COUNT) return;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mats[i].tex);
}

void MaterialLib::destroy() {
    if (forestGroundTex && forestGroundTex != mats[MAT_GROUND].tex)
        destroyTexture(forestGroundTex);
    forestGroundTex = 0;
    for (int i = 0; i < MAT_COUNT; i++) {
        destroyTexture(mats[i].tex);
        mats[i].tex = 0;
    }
}
