#pragma once
#include "gl_loader.h"

// Upload an RGB8 tileable image. Returns GL texture id (caller owns); 0 on failure.
GLuint uploadTextureRGB(const unsigned char* px, int w, int h);

// Load an image file (PNG/JPG/...) as an RGB8 tileable texture. Returns 0 if the
// file is missing/unreadable, so callers can fall back to a procedural texture.
GLuint loadTexture(const char* path);
// Load an RGBA texture for alpha-tested/blended atlases such as environment decals.
GLuint loadTextureRGBA(const char* path);

// Cached satellite-style map from the active heightfield, habitats, real tree
// crowns and static geometry. GL bottom row is -Z; north (+Z) is at the top.
// Bake at map setup only; no per-frame generation or extra scene rendering.
struct Box;
GLuint makeMapTexture(const Box* boxes, int count, float worldHalf);

// Procedural seamless 256×256 tiles (no PNG files required).
GLuint makeGroundTexture();
GLuint makeConcreteTexture();
GLuint makeMetalTexture();
GLuint makeWoodTexture();
GLuint makeRockTexture();
GLuint makeDirtTexture();

void destroyTexture(GLuint tex);
