#pragma once
#include "gl_loader.h"

// Upload an RGB8 tileable image. Returns GL texture id (caller owns); 0 on failure.
GLuint uploadTextureRGB(const unsigned char* px, int w, int h);

// Procedural seamless 256×256 tiles (no PNG files required).
GLuint makeGroundTexture();
GLuint makeConcreteTexture();
GLuint makeMetalTexture();
GLuint makeWoodTexture();
GLuint makeRockTexture();

void destroyTexture(GLuint tex);
