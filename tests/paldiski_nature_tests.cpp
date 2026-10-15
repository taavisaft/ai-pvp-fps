#include "map.h"
#include "tree_scatter.h"
#include "training_woodland.h"
#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while (0)

int main() {
    setMap(MAP_PALDISKI);
    CHECK(gArenaHalf == 1024.0f);
    CHECK(gMapBoxCount == 0 && gTownBoxCount == 0);
    CHECK(gTerrainPadCount == 0);
    CHECK(gKhrushBoxCount == 0 && gNatureFenceCount == 0 && gNatureLogCount == 0);
    CHECK(gMapSpawnCount >= 16);
    CHECK(gTrees.size() > 10000 && gTrees.size() == gTreeCols.size());
    int species[TRAINING_TREE_TYPES]{}, quadrants[4]{};
    for (size_t i=0; i<gTrees.size(); ++i) {
        const auto& t=gTrees[i];
        CHECK(fabsf(t.x)<=gArenaHalf && fabsf(t.z)<=gArenaHalf);
        CHECK(t.y+.15f >= 1.6f);
        CHECK(fabsf(t.y+.15f-terrainHeight(t.x,t.z)) < .001f);
        CHECK(gTreeCols[i].x==t.x && gTreeCols[i].z==t.z);
        CHECK(!paldiskiSpawnClearance(t.x,t.z,2.5f));
        ++species[taigaTreeType(t.x,t.z,pineForestBiome(t.x,t.z))];
        ++quadrants[(t.x>0 ? 1 : 0)+(t.z>0 ? 2 : 0)];
    }
    for(int count:species) CHECK(count>0);
    for(int count:quadrants) CHECK(count>100);
    for(int i=0;i<gMapSpawnCount;++i) {
        const auto& p=gMapSpawns[i];
        CHECK(terrainHeight(p.x,p.z)>=1.0f);
        for(const auto& t:gTreeCols)
            CHECK(glm::length(glm::vec2(p.x-t.x,p.z-t.z))>t.r+.4f);
    }
    // Every placement and collider must survive a lobby round trip unchanged.
    const auto trees=gTrees;
    setMap(MAP_LOBBY);
    CHECK(gMapBoxCount>0);
    setMap(MAP_PALDISKI);
    CHECK(gMapBoxCount==0 && gTerrainPadCount==0);
    CHECK(gTrees.size()==trees.size());
    for(size_t i=0;i<trees.size() && i<gTrees.size();++i) {
        const auto& a=trees[i]; const auto& b=gTrees[i];
        CHECK(a.x==b.x && a.y==b.y && a.z==b.z);
        CHECK(a.scale==b.scale && a.yaw==b.yaw && a.tint==b.tint);
    }
    return failures ? 1 : 0;
}
