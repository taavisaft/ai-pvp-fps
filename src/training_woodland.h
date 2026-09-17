#pragma once
#include <cstdint>
#include <cmath>
#include "training_spruce.h"

constexpr int TRAINING_TREE_TYPES=7;
inline constexpr const char* TRAINING_TREE_NAMES[]={
    "full-spruce","narrow-spruce","drooping-spruce","high-crown-spruce",
    "ash","birch","oak"
};
inline float trainingTreeWidth(int type) {
    return type<4 ? TRAINING_SPRUCE_WIDTH : type==5 ? .94f : 1.30f;
}
inline uint32_t trainingTreeHash(int32_t a,int32_t b) {
    uint32_t n=uint32_t(a)*374761393u+uint32_t(b)*668265263u;
    n=(n^(n>>13))*1274126177u;
    return n^(n>>16);
}
inline int trainingTreeType(float x,float z) {
    uint32_t tree=trainingTreeHash(int32_t(floorf(x*8)),int32_t(floorf(z*8)));
    if(z>75 && fabsf(x)<90) {
        if(x<-30 && z<200) return 0;
        return 4+int((tree/10)%3);
    }
    uint32_t stand=trainingTreeHash(int32_t(floorf(x/40))+911,int32_t(floorf(z/40))-577);
    uint32_t pick=tree%4==0 ? tree/16 : stand;
    float reach=fmaxf(fabsf(x)*1.3f,fabsf(z));
    float t=fminf(1.0f,fmaxf(0.0f,(reach-250.0f)/270.0f));
    float conifer=.30f+.55f*t*t*(3-2*t);
    if(float(pick%1000)*.001f<conifer) return int((pick/1000)%4);
    return 4+int((pick/1000)%3);
}
void vegBuildBroadleaf(std::vector<float>& v,std::vector<unsigned>& idx,int species,bool low=false);
