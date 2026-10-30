#pragma once
#include <cstdint>
#include <cmath>
#include "training_spruce.h"
#include "nature_preview.h"

constexpr int TRAINING_TREE_TYPES=10;
inline constexpr const char* TRAINING_TREE_NAMES[]={
    "full-spruce","narrow-spruce","drooping-spruce","high-crown-spruce",
    "ash","birch","oak","pine","imported-spruce","imported-spruce-243"
};
inline bool trainingConifer(int type) { return type<4 || type>=7; }
inline int trainingTreeShader(int type) { return type>=8 ? 6 : type==7 ? 5 : type<4 ? 1 : type-2; }
inline const char* trainingImportedPath(int type) { return type==9 ? "spruce243" : "spruce"; }
inline float trainingTreeWidth(int type) {
    return type==9 ? .62f : type==8 ? .45f : type==7 ? .94f : type<4 ? TRAINING_SPRUCE_WIDTH : type==5 ? 1.06f : 1.46f;
}
inline uint32_t trainingTreeHash(int32_t a,int32_t b) {
    uint32_t n=uint32_t(a)*374761393u+uint32_t(b)*668265263u;
    n=(n^(n>>13))*1274126177u;
    return n^(n>>16);
}
inline int trainingTreeType(float x,float z) {
    int sample=naturePreviewTreeType(x,z);
    if(sample>=0) return sample;
    return 8+int(trainingTreeHash(int32_t(floorf(x*.1f)),int32_t(floorf(z*.1f)))%2);
}
inline int taigaTreeType(float x,float z,float forest) {
    uint32_t tree=trainingTreeHash(int32_t(floorf(x*8)),int32_t(floorf(z*8)));
    uint32_t stand=trainingTreeHash(int32_t(floorf(x/40))+911,int32_t(floorf(z/40))-577);
    uint32_t pick=tree%4==0 ? tree/16 : stand;
    // Leafy meadow edges transition into spruce-dominated forest interiors.
    float conifer=.45f+.45f*fminf(1.0f,fmaxf(0.0f,forest));
    if(float(pick%1000)*.001f<conifer) return int((pick/1000)%4);
    uint32_t leaf=(pick/1000)%10;
    return leaf<6 ? 5 : leaf<8 ? 4 : 6;
}
void vegBuildBroadleaf(std::vector<float>& v,std::vector<unsigned>& idx,int species,bool low=false);
