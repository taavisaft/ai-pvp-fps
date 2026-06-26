#pragma once
#include "mesh.h"
#include "gl_loader.h"
#include "map.h"   // CityBuilding

// Procedural Soviet-panel ("Plattenbau") facade: one repeating window-panel cell,
// GL_REPEAT, sampled by UV so a building tiles it baysX x floors. Returns a GL texture
// (0 on failure). No assets — swap in textures/facade.png later through the same unit.
GLuint makeFacadeTexture();

// Build the four outward-facing facade walls of one building as a UV-tiled mesh
// (pos+normal+uv). Centered on X/Z at the origin, base at y=0, top at y=h — the
// caller translates by the building's footprint center.
bool buildFacadeMesh(Mesh& out, const CityBuilding& b);
