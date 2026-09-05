#include "map.h"
#include "physics.h"
#include "tree_scatter.h"
#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while (0)

static bool obstructed(glm::vec3 a, glm::vec3 b) {
    float t;
    for (int i = 0; i < gMapBoxCount; ++i)
        if (segmentAabb(a, b, gMapBoxes[i].center, gMapBoxes[i].half, t)) return true;
    return false;
}
int main() {
    setMap(MAP_PALDISKI);
    const float x = FOREST_SITE_X, z = FOREST_SITE_Z, h = terrainHeight(x, z);
    CHECK(gMapBoxCount > 50 && gMapBoxCount < 150);
    CHECK(gTerrainPadCount == 1);
    CHECK(!obstructed({x,h+1.6f,z-8}, {x,h+1.6f,z+8})); // both doors
    CHECK(!obstructed({x-6,h+1.6f,z}, {x+6,h+1.6f,z})); // windows
    CHECK(obstructed({x-6,h+0.6f,z}, {x+6,h+0.6f,z})); // sill
    CHECK(obstructed({x-2,h+1.6f,z-8}, {x-2,h+1.6f,z+8})); // wall
    CHECK(obstructed({x,h+5,z}, {x,h+1,z})); // roof
    // Walk through both doorways using the actual shared movement/collision path.
    Player p{};
    p.pos = {x,h,z-8};
    InputState in{}; in.w = true; in.yaw = 90;
    for (int i = 0; i < 240; ++i) movePlayer(p, in, 1.0f/60);
    CHECK(p.pos.z > z+7);
    CHECK(fabsf(p.pos.y-terrainHeight(p.pos.x,p.pos.z)) < 0.1f);
    // A wall blocks the same route when shifted left.
    p = Player{}; p.pos = {x-2,h,z-8};
    for (int i = 0; i < 180; ++i) movePlayer(p, in, 1.0f/60);
    CHECK(p.pos.z < z-3);
    for (const auto& tree : gTrees) CHECK(!forestSiteClearance(tree.x,tree.z,1));
    glm::vec3 spawn = gMapSpawns[0]; spawn.y = terrainHeight(spawn.x,spawn.z);
    CHECK(!obstructed(spawn+glm::vec3(0,.1f,0),spawn+glm::vec3(0,1.8f,0)));
    for (const auto& t : gTreeCols)
        CHECK(glm::length(glm::vec2(spawn.x-t.x,spawn.z-t.z)) > t.r+.4f);
    // Same regeneration, no accumulating geometry or pads; lobby switching resets.
    const int boxes = gMapBoxCount, trees = (int)gTrees.size();
    Box first = gMapBoxes[0];
    setMap(MAP_LOBBY); CHECK(gTerrainPadCount == 0);
    setMap(MAP_PALDISKI);
    CHECK(gMapBoxCount == boxes && (int)gTrees.size() == trees);
    CHECK(gMapBoxes[0].center == first.center && gMapBoxes[0].half == first.half);
    return failures ? 1 : 0;
}
