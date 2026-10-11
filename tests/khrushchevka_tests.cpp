#include "map.h"
#include "physics.h"
#include "tree_scatter.h"
#include <cstdio>

static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)
static bool blocked(glm::vec3 a,glm::vec3 b) {
    a+=glm::vec3(KHRUSH_X,KHRUSH_Y,KHRUSH_Z);
    b+=glm::vec3(KHRUSH_X,KHRUSH_Y,KHRUSH_Z);
    float t;
    for(int i=gKhrushBoxFirst;i<gKhrushBoxFirst+gKhrushBoxCount;++i)
        if(segmentAabb(a,b,gMapBoxes[i].center,gMapBoxes[i].half,t)) return true;
    return false;
}
int main() {
    setMap(MAP_LOBBY);
    CHECK(gKhrushBoxCount>1800);
    CHECK(gMapBoxCount<MAX_MAP_BOXES-100);
    CHECK(gKhrushBoxFirst+gKhrushBoxCount==gNatureFenceFirst);
    CHECK(gNatureFenceFirst+gNatureFenceCount==gMapBoxCount);
    for(int s=0;s<KHRUSH_SECTIONS;++s) {
        float left=-KHRUSH_LENGTH*.5f+s*KHRUSH_SECTION, entry=left+7.1f;
        // All entrance openings are clear at standing height; piers are solid.
        CHECK(!blocked({entry,1.7f,7.5f},{entry,1.7f,4.8f}));
        CHECK(blocked({entry+1.2f,1.7f,7.5f},{entry+1.2f,1.7f,4.8f}));
        CHECK(blocked({entry,2.7f,7.5f},{entry,2.7f,4.8f}));
        CHECK(blocked({left+1.3f,.5f,-7.5f},{left+1.3f,.5f,-4.8f}));
        for(int f=0;f<KHRUSH_FLOORS;++f) {
            // Ground and all upper-floor slabs support the future apartment area.
            float y=f==0 ? -.09f : .10f+f*KHRUSH_FLOOR;
            CHECK(blocked({left+1.3f,y+.4f,0},{left+1.3f,y-.4f,0}));
        }
        CHECK(blocked({entry,16,0},{entry,13.7f,0}));
        // Exercise actual standing movement through the full entrance threshold.
        Player p{}; p.pos={entry,KHRUSH_Y,KHRUSH_Z+9};
        InputState in{}; in.w=true; in.yaw=-90;
        for(int i=0;i<75;++i) movePlayer(p,in,1.f/60);
        CHECK(p.pos.z<KHRUSH_Z+5.6f);
        CHECK(p.pos.y>=KHRUSH_Y-.01f);
    }
    // End wall has two columns of real window apertures, solid central pier.
    CHECK(blocked({-58,1.7f,0},{-55,1.7f,0}));
    CHECK(blocked({-58,.5f,-3.77f},{-55,.5f,-3.77f}));
    for(const auto& t:gTrees) CHECK(!khrushchevkaClearance(t.x,t.z,3));
    for(float x:{-56.f,0.f,56.f}) for(float z:{-115.f,-110.f,-105.f})
        CHECK(fabsf(terrainHeight(x,z)-KHRUSH_Y)<.001f);
    // The new terrain apron joins the original landscape without abrupt steps.
    for(int z=-170;z<-60;++z) for(int x=-110;x<110;++x) {
        float h=terrainHeight(float(x),float(z));
        CHECK(std::isfinite(h));
        CHECK(fabsf(terrainHeight(x+1.f,float(z))-h)<1.5f);
        CHECK(fabsf(terrainHeight(float(x),z+1.f)-h)<1.5f);
    }
    int count=gMapBoxCount;
    Box first=gMapBoxes[gKhrushBoxFirst];
    setMap(MAP_PALDISKI);
    CHECK(gKhrushBoxCount==0);
    CHECK(gMapBoxCount<150);
    setMap(MAP_LOBBY);
    CHECK(gMapBoxCount==count);
    CHECK(gMapBoxes[gKhrushBoxFirst].center==first.center);
    CHECK(gMapBoxes[gKhrushBoxFirst].half==first.half);
    printf("[building test] %d shared colliders / %d capacity\n",gKhrushBoxCount,MAX_MAP_BOXES);
    return failures ? 1 : 0;
}
