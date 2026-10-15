#include "satellite_map.h"
#include "map.h"
#include "tree_scatter.h"
#include <vector>
#include <cstdio>

static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)

int main() {
    constexpr int side=256;
    std::vector<uint8_t> rgb(side*side*3),base(rgb.size());
    std::vector<float> heights(side*side);
    CHECK(!bakeSatelliteMap(nullptr,heights.data(),side,128,nullptr,0));
    CHECK(!bakeSatelliteMap(rgb.data(),heights.data(),1,128,nullptr,0));
    CHECK(!bakeSatelliteMap(rgb.data(),heights.data(),side,0,nullptr,0));
    CHECK(!bakeSatelliteMap(rgb.data(),heights.data(),side,128,nullptr,1));
    setMap(MAP_PALDISKI);
    CHECK(bakeSatelliteMap(rgb.data(),heights.data(),side,gArenaHalf,nullptr,0));
    CHECK(bakeSatelliteMap(base.data(),heights.data(),side,gArenaHalf,nullptr,0));
    CHECK(rgb==base);
    int wet=0,dry=0;
    for(int z=0;z<side;z+=7) for(int x=0;x<side;x+=7) {
        int i=z*side+x;
        float wx=-gArenaHalf+(x+.5f)*2*gArenaHalf/side;
        float wz=-gArenaHalf+(z+.5f)*2*gArenaHalf/side;
        CHECK(heights[i]==terrainHeight(wx,wz));
        if(wx<-900) {
            CHECK(heights[i]<0);
            CHECK(rgb[i*3+2]>rgb[i*3]);
            ++wet;
        }
        if(wx>0 && heights[i]>1) ++dry;
    }
    CHECK(wet>50 && dry>100);

    // Isolate a real crown at an asymmetric location: +Z must be the upper UV
    // row, and X/Z must not be transposed. Adding a tree cannot move the ground.
    setMap(MAP_LOBBY);
    gTrees.clear();
    CHECK(bakeSatelliteMap(base.data(),heights.data(),side,128,nullptr,0));
    auto ground=heights;
    gTrees.push_back({37,terrainHeight(37,63)-.15f,63,24,0,1});
    CHECK(bakeSatelliteMap(rgb.data(),heights.data(),side,128,nullptr,0));
    CHECK(heights==ground);
    auto changed=[&](int x,int z) {
        int i=((z+128)*side+(x+128))*3;
        return rgb[i]!=base[i] || rgb[i+1]!=base[i+1] || rgb[i+2]!=base[i+2];
    };
    CHECK(changed(37,63));
    CHECK(!changed(37,-63));
    CHECK(!changed(63,37));
    CHECK(!changed(-37,63));

    gTrees.clear();
    Box roof{{-51,25,17},{6,2,4}};
    CHECK(bakeSatelliteMap(rgb.data(),heights.data(),side,128,&roof,1));
    CHECK(changed(-51,17));
    CHECK(!changed(-51,-17));

    // The lobby pond must be water even though it is above global sea level.
    CHECK(bakeSatelliteMap(rgb.data(),heights.data(),side,256,nullptr,0));
    int pond=(int((TRAINING_POND_Z+256)/2)*side+128)*3;
    CHECK(rgb[pond+2]>rgb[pond]);
    return failures ? 1 : 0;
}
