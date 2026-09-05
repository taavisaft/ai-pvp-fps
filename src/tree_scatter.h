#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "spatial.h"

// Deterministic tree placement, shared by server and client (GL-free). This is the
// SINGLE SOURCE OF TRUTH for where spruces stand: the client builds its rendered
// instances from gTrees, and both server + client build collision cylinders from the
// same scatter — so a tree you see is a tree you bump, with no sync. Rebuilt from the
// shared terrain/map noise in setMap() on every map change (same math every launch).
//
// Placement math used to live in Vegetation::buildTrees (client-only, GL-coupled);
// it moved here so the headless server can run identical collision.

struct TreeInstance { float x, y, z, scale, yaw, tint; };  // matches Vegetation::Tree
struct TreeCol      { float x, z, r; };                    // XZ trunk cylinder

extern std::vector<TreeInstance> gTrees;       // full render/placement list
extern std::vector<TreeCol>      gTreeCols;    // collision cylinders (x,z,radius)
extern float gTreeTrunkMaxRadius; // broadphase padding, computed at map startup
extern SpatialGrid               gTreeColGrid; // XZ grid over gTreeCols for queries

// Trunk collision radius from tree height: modest base + slight growth with size, so
// mature spruces have a fatter base flare than saplings. Tuned for a "bump the trunk"
// feel, not the full canopy.
inline float treeCollRadius(float scale) { return 0.30f + 0.025f * scale; }

// Run the deterministic scatter for the active map (gMapId) and (re)fill gTrees,
// gTreeCols and gTreeColGrid. Called by setMap(); safe to call again on map change.
void buildTreeColliders();
