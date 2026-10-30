#pragma once
#include <cstdint>
#include <cmath>
#include "training_spruce.h"
#include "nature_preview.h"

constexpr int TRAINING_TREE_TYPES=17;
inline constexpr const char* TRAINING_TREE_NAMES[]={
    "full-spruce","narrow-spruce","drooping-spruce","high-crown-spruce",
    "ash","birch","oak","pine","imported-spruce","imported-spruce-243",
    "elm-002","elm-005","elm-006","elm-007","elm-008","elm-009","elm-010"
};
inline bool trainingConifer(int type) { return type<4 || (type>=7 && type<=9); }
inline int trainingTreeShader(int type) { return type>=10 ? 7 : type>=8 ? 6 : type==7 ? 5 : type<4 ? 1 : type-2; }
inline int trainingImportedTextureSlot(int type) { return type>=10 ? 2 : type-8; }
inline const char* trainingImportedPath(int type) {
    constexpr const char* paths[]={"spruce","spruce243","elm_002","elm_005","elm_006","elm_007","elm_008","elm_009","elm_010"};
    return paths[type-8];
}
inline float trainingTreeWidth(int type) {
    if(type>=10) {
        constexpr float widths[]={1.49f,1.42f,1.16f,1.18f,1.30f,1.14f,1.65f};
        return widths[type-10];
    }
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
    uint32_t stand=trainingTreeHash(int32_t(floorf(x*.05f)),int32_t(floorf(z*.05f)));
    return stand%4<2 ? 8+int(stand%2) : 10+int((stand/4)%7);
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
