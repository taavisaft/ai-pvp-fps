#pragma once
#include "training_spruce.h"

constexpr int TRAINING_TREE_TYPES=7;
inline constexpr const char* TRAINING_TREE_NAMES[]={
    "full-spruce","narrow-spruce","drooping-spruce","high-crown-spruce",
    "ash","birch","oak"
};
inline float trainingTreeWidth(int type) {
    return type<4 ? TRAINING_SPRUCE_WIDTH : type==5 ? .94f : 1.30f;
}
inline int trainingTreeType(float x,float z) {
    uint32_t n=uint32_t(int32_t(floorf(x*8)))*374761393u
              +uint32_t(int32_t(floorf(z*8)))*668265263u;
    n=(n^(n>>13))*1274126177u; n^=n>>16;
    return n%10<5 ? int(n%4) : 4+int((n/10)%3);
}
void vegBuildBroadleaf(std::vector<float>& v,std::vector<unsigned>& idx,int species);
