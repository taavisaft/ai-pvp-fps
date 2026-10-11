#pragma once
#include <cmath>

// Offline nature catalogue, east of the (0,136) spawn. One size per asset.
struct NatureTreeSample { float x, z; };
inline constexpr NatureTreeSample NATURE_TREES[] = {
    {18,114}, {32,114}, {46,114}, {60,114}, // four spruce forms
    {18,136}, {32,136}, {46,136}           // ash, birch, oak
};
inline constexpr float NATURE_TREE_HEIGHT = 10.0f;
inline constexpr float NATURE_BUSH_X = 10, NATURE_BUSH_Z = 126;
inline constexpr float NATURE_MEADOW_X = 10, NATURE_MEADOW_Z = 116;
inline bool naturePreviewArea(float x,float z) {
    return x>6 && x<70 && z>92 && z<148;
}
inline int naturePreviewTreeType(float x,float z) {
    for(int i=0;i<7;++i)
        if(x==NATURE_TREES[i].x && z==NATURE_TREES[i].z) return i;
    return -1;
}
// Four 6 m surface patches: meadow, dirt, rock, forest floor.
// Feathering is mirrored in basic.frag; bare patches suppress streamed grass.
inline float naturePreviewBareGround(float x,float z) {
    float strength=0;
    for(int i=1;i<4;++i) {
        float dx=x-(18+14*i), dz=z-100;
        float t=fmaxf(0,fminf(1,(sqrtf(dx*dx+dz*dz)-2.5f)/.5f));
        strength=fmaxf(strength,1-t*t*(3-2*t));
    }
    return strength;
}
