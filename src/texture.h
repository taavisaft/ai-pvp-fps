#pragma once
#include "gl_loader.h"

// Upload an RGB8 tileable image. Returns GL texture id (caller owns); 0 on failure.
GLuint uploadTextureRGB(const unsigned char* px, int w, int h);

// Load an image file (PNG/JPG/...) as an RGB8 tileable texture. Returns 0 if the
// file is missing/unreadable, so callers can fall back to a procedural texture.
GLuint loadTexture(const char* path);

// Top-down "satellite" map texture baked from the arena box layout: muted terrain
// noise + box footprints shaded by height with drop shadows. Spans world XZ in
// [-worldHalf, worldHalf]^2, north (+Z) at the top row. boxes may be null (count 0).
// Used as a fallback when no hand-made textures/map_<name>.png exists.
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
