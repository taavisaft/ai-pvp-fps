#pragma once
#include <cstdint>
struct Box;

// Startup/map-change bake only. Both buffers contain side*side pixels; RGB is
// tightly packed. Row 0 is -Z (GL bottom), +X right and +Z north/up, matching HUD UVs.
// Samples the active shared terrain, tree scatter and static collision geometry.
bool bakeSatelliteMap(uint8_t* rgb, float* heights, int side, float half,
                      const Box* boxes, int count);
