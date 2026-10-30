#include "training_woodland.h"
#include "tree_collision.h"
#include <cstdio>
#include "tree_attachment_checks.h"

static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)
int main() {
    float radius[3]{};
    std::vector<float> spruce; std::vector<unsigned> spruceIdx;
    vegBuildTrainingSpruce(spruce,spruceIdx,0);
    for(int species=0;species<3;++species) {
        std::vector<float> v; std::vector<unsigned> idx;
        vegBuildBroadleaf(v,idx,species);
        CHECK(checkTreeAttachments(v,idx,species)==0);
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
        std::vector<float> low; std::vector<unsigned> lowIdx;
        vegBuildBroadleaf(low,lowIdx,species,true);
        CHECK(checkTreeAttachments(low,lowIdx,species)==0);
        CHECK(lowIdx.size()<idx.size());
        // The lobby's single mesh must retain every near-mesh leaf spray.
        std::vector<float> nearLeaves, farLeaves;
        for(size_t i=0;i<v.size();i+=12) if(v[i+10]>=0)
            nearLeaves.insert(nearLeaves.end(),v.begin()+i,v.begin()+i+12);
        for(size_t i=0;i<low.size();i+=12) if(low[i+10]>=0)
            farLeaves.insert(farLeaves.end(),low.begin()+i,low.begin()+i+12);
        CHECK(nearLeaves==farLeaves);
        // The proof must reject the old failure: a displaced, unsupported card.
        for(size_t i=0;i<v.size();i+=12) if(v[i+10]>=0) {
            for(size_t j=i;j<i+6*12;j+=12) v[j]+=1.0f;
            break;
        }
        CHECK(checkTreeAttachments(v,idx,species)>0);
    }
    CHECK(radius[1]<radius[0] && radius[0]<radius[2]);
    int counts[TRAINING_TREE_TYPES]{};
    int sameNeighbour=0, samples=0, farSpruce=0, farSamples=0;
    for(int x=-900;x<=900;x+=7) for(int z=-900;z<=900;z+=7) {
        int type=trainingTreeType(float(x),float(z));
        CHECK(type>=0 && type<TRAINING_TREE_TYPES);
        if(type>=0 && type<TRAINING_TREE_TYPES) ++counts[type];
        sameNeighbour+=type==trainingTreeType(x+5.0f,z+3.0f); ++samples;
        if(abs(z)>600) { farSpruce+=trainingConifer(type); ++farSamples; }
    }
    for(int type=0;type<TRAINING_TREE_TYPES;++type)
        CHECK(type>=8 ? counts[type]>samples/3 : counts[type]==0);
    CHECK(sameNeighbour*2>samples);
    CHECK(farSpruce==farSamples);
    int taiga[TRAINING_TREE_TYPES]{}, denseSpruce=0, denseSamples=0, edgeBirch=0, edgeLeaf=0;
    for(int x=-900;x<=900;x+=7) for(int z=-900;z<=900;z+=7) {
        int dense=taigaTreeType(float(x),float(z),1.0f), edge=taigaTreeType(float(x),float(z),0.0f);
        CHECK(dense>=0 && dense<TRAINING_TREE_TYPES && edge>=0 && edge<TRAINING_TREE_TYPES);
        ++taiga[edge]; denseSpruce+=dense<4; ++denseSamples;
        if(edge>=4) { ++edgeLeaf; edgeBirch+=edge==5; }
    }
    for(int type=0;type<TRAINING_TREE_TYPES;++type)
        CHECK(type>=7 ? taiga[type]==0 : taiga[type]>200);
    CHECK(denseSpruce*100>denseSamples*90);
    CHECK(edgeBirch*2>edgeLeaf);
    std::vector<float> pine, pineLow; std::vector<unsigned> pineIdx, pineLowIdx;
    vegBuildTrainingSpruce(pine,pineIdx,4);
    vegBuildTrainingSpruce(pineLow,pineLowIdx,4,true);
    CHECK(checkTreeAttachments(pine,pineIdx,-1)==0);
    CHECK(checkTreeAttachments(pineLow,pineLowIdx,-1)==0);
    CHECK(!pineIdx.empty() && pineLowIdx.size()<pineIdx.size());
    for(size_t i=0;i<pine.size();i+=12) {
        for(int j=0;j<12;++j) CHECK(std::isfinite(pine[i+j]));
        float r=sqrtf(pine[i]*pine[i]+pine[i+2]*pine[i+2]);
        CHECK(r*1.095f<trainingTreeWidth(7)*.5f);
        if(pine[i+10]>=0) CHECK(pine[i+1]>.40f);
        if(i<(TREE_TRUNK_SIDES*2+2)*12)
            for(int j=0;j<6;++j) CHECK(pine[i+j]==spruce[i+j]);
    }
    return failures ? 1 : 0;
}
