#pragma once
#include "renderer.h"

inline void bindFlatColor(Shader& sh) {
    sh.setInt(sh.locUseTex, 0);
}

inline void bindMaterial(Shader& sh, const MaterialLib& lib, MaterialId id) {
    const Material& m = lib.mats[(int)id];
    lib.bind(id);
    sh.setInt(sh.locUseTex, 1);
    sh.setFloat(sh.locTile, m.tile);
    sh.setFloat(sh.locSpec, m.spec);
    sh.setVec3(sh.locTint, m.tint);
}
