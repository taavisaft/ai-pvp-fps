#include "training_spruce.h"
#include "tree_collision.h"
#include <cstdio>

static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)
int main() {
    float radius[4]{},foliageBase[4];
    std::vector<float> original;
    for(int type=0;type<TRAINING_SPRUCE_TYPES;++type) {
        std::vector<float> v; std::vector<unsigned> idx;
        vegBuildTrainingSpruce(v,idx,type);
        CHECK(v.size()%12==0 && idx.size()%3==0 && !idx.empty());
        CHECK(idx.size()/3<8000);
        for(unsigned i:idx) CHECK(i<v.size()/12);
        foliageBase[type]=1;
        for(size_t i=0;i<v.size();i+=12) {
            for(int a=0;a<12;++a) CHECK(std::isfinite(v[i+a]));
            float r=sqrtf(v[i]*v[i]+v[i+2]*v[i+2]);
            CHECK(r<TRAINING_SPRUCE_WIDTH*.5f/1.095f);
            CHECK(v[i+1]>-.05f && v[i+1]<1.10f);
            if(v[i+10]>=0) {
                radius[type]=fmaxf(radius[type],r);
                foliageBase[type]=fminf(foliageBase[type],v[i+1]);
            }
        }
        // Every type keeps the exact six-sided collision trunk, including caps.
        if(type==0) original=v;
        for(int i=0;i<(TREE_TRUNK_SIDES*2+2)*12;++i) CHECK(v[i]==original[i]);
        for(int j=0;j<TREE_TRUNK_SIDES;++j) {
            float a=j*6.2831853f/TREE_TRUNK_SIDES;
            CHECK(fabsf(v[j*24]-cosf(a)*TREE_TRUNK_BASE)<1e-6f);
            CHECK(fabsf(v[j*24+12+1]-TREE_TRUNK_HEIGHT)<1e-6f);
        }
        std::vector<float> again; std::vector<unsigned> againIdx;
        vegBuildTrainingSpruce(again,againIdx,type);
        CHECK(v==again && idx==againIdx);
    }
    CHECK(radius[1]<radius[0] && radius[0]<radius[2]);
    CHECK(foliageBase[3]>.30f && foliageBase[3]>foliageBase[0]+.15f);
    int counts[4]{};
    for(int x=-60;x<=60;x+=5) for(int z=-60;z<=60;z+=5) {
        int type=trainingSpruceType(float(x),float(z));
        CHECK(type>=0 && type<4);
        if(type>=0 && type<4) ++counts[type];
    }
    for(int count:counts) CHECK(count>80);
    return failures ? 1 : 0;
}
