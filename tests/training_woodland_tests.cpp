#include "training_woodland.h"
#include "tree_collision.h"
#include <cstdio>

static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)
int main() {
    float radius[3]{};
    std::vector<float> spruce; std::vector<unsigned> spruceIdx;
    vegBuildTrainingSpruce(spruce,spruceIdx,0);
    for(int species=0;species<3;++species) {
        std::vector<float> v; std::vector<unsigned> idx;
        vegBuildBroadleaf(v,idx,species);
        CHECK(!idx.empty() && v.size()%12==0 && idx.size()%3==0);
        CHECK(idx.size()/3<8000);
        for(unsigned i:idx) CHECK(i<v.size()/12);
        for(size_t i=0;i<v.size();i+=12) {
            for(int j=0;j<12;++j) CHECK(std::isfinite(v[i+j]));
            float r=sqrtf(v[i]*v[i]+v[i+2]*v[i+2]);
            radius[species]=fmaxf(radius[species],r);
            CHECK(r*1.095f<trainingTreeWidth(4+species)*.5f);
            CHECK(v[i+1]>=0 && v[i+1]<1.10f);
            if(v[i+10]>=0) {
                CHECK(v[i+10]>float(species)/3 && v[i+10]<float(species+1)/3);
                CHECK(v[i+11]>0 && v[i+11]<1);
            }
        }
        // Bark palette changes; trunk positions and collision surface do not.
        for(int i=0;i<TREE_TRUNK_SIDES*2+2;++i)
            for(int j=0;j<6;++j) CHECK(v[i*12+j]==spruce[i*12+j]);
        std::vector<float> again; std::vector<unsigned> againIdx;
        vegBuildBroadleaf(again,againIdx,species);
        CHECK(v==again && idx==againIdx);
    }
    CHECK(radius[1]<radius[0] && radius[0]<radius[2]);
    int counts[TRAINING_TREE_TYPES]{};
    int sameNeighbour=0, samples=0, farSpruce=0, farSamples=0;
    for(int x=-900;x<=900;x+=7) for(int z=-900;z<=900;z+=7) {
        int type=trainingTreeType(float(x),float(z));
        CHECK(type>=0 && type<TRAINING_TREE_TYPES);
        if(type>=0 && type<TRAINING_TREE_TYPES) ++counts[type];
        sameNeighbour+=type==trainingTreeType(x+5.0f,z+3.0f); ++samples;
        if(abs(z)>600) { farSpruce+=type<4; ++farSamples; }
    }
    for(int n:counts) CHECK(n>1000);
    CHECK(sameNeighbour*2>samples);
    CHECK(farSpruce*10>farSamples*7);
    for(int z=80;z<200;z+=5) CHECK(trainingTreeType(-50,float(z))==0);
    for(int z=80;z<300;z+=5) CHECK(trainingTreeType(40,float(z))>=4);
    return failures ? 1 : 0;
}
