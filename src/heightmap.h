#pragma once

// Loads a 16-bit grayscale heightmap PNG into terrain.h's sampler globals, so the
// same designed terrain drives client rendering/collision AND server collision. Values
// are normalized (min..max -> 0..amplitude metres) and bilinear-sampled identically on
// both ends (see terrain.h), keeping the simulation deterministic with no network sync.
//
// Returns false if no file is found at any candidate path; terrain.h then falls back to
// its procedural noise, so the game still runs.
bool loadHeightmap(const char* primaryPath, float amplitudeMeters, float worldHalf);
void freeHeightmap();
