#include "material.h"
#include "texture.h"
#include <cstdio>

bool MaterialLib::init() {
    mats[MAT_GROUND].tex   = makeGroundTexture();
    mats[MAT_CONCRETE].tex = makeConcreteTexture();
    mats[MAT_METAL].tex    = makeMetalTexture();
    mats[MAT_WOOD].tex     = makeWoodTexture();
    mats[MAT_ROCK].tex     = makeRockTexture();

    mats[MAT_GROUND].tint   = {1.0f, 1.0f, 1.0f};
    mats[MAT_CONCRETE].tint = {1.0f, 1.0f, 1.0f};
    mats[MAT_METAL].tint    = {0.85f, 0.88f, 0.92f};
    mats[MAT_WOOD].tint     = {1.0f, 1.0f, 1.0f};
    mats[MAT_ROCK].tint     = {1.0f, 1.0f, 1.0f};

    mats[MAT_GROUND].tile   = 2.0f;
    mats[MAT_CONCRETE].tile = 1.0f;
    mats[MAT_METAL].tile    = 1.0f;
    mats[MAT_WOOD].tile     = 0.8f;
    mats[MAT_ROCK].tile     = 1.5f;

    mats[MAT_GROUND].spec   = 0.0f;
    mats[MAT_CONCRETE].spec = 0.05f;
    mats[MAT_METAL].spec    = 0.35f;
    mats[MAT_WOOD].spec     = 0.02f;
    mats[MAT_ROCK].spec     = 0.04f;

    for (int i = 0; i < MAT_COUNT; i++) {
        if (!mats[i].tex) {
            fprintf(stderr, "material: failed to create texture %d\n", i);
            destroy();
            return false;
        }
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
    for (int i = 0; i < MAT_COUNT; i++) {
        destroyTexture(mats[i].tex);
        mats[i].tex = 0;
    }
}
