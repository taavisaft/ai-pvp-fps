#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

constexpr int TRAINING_SPRUCE_TYPES=4;
constexpr float TRAINING_SPRUCE_WIDTH=.76f;

// Cosmetic assignment only: stable across camera movement, LOD and shadows.
inline int trainingSpruceType(float x,float z) {
    uint32_t n=uint32_t(int32_t(floorf(x*8)))*374761393u
              +uint32_t(int32_t(floorf(z*8)))*668265263u;
    n=(n^(n>>13))*1274126177u; n^=n>>16;
    return int(n%TRAINING_SPRUCE_TYPES);
}

struct TrainingSpruceProfile {
    const char* name;
    int tiers,branches,twigs;
    float crownBase,radius,taper,droop,tint;
};
inline constexpr TrainingSpruceProfile TRAINING_SPRUCE_PROFILES[] = {
    {"full",16,5,4,.17f,.225f,.82f,1.0f,1.0f},
    {"narrow",18,4,3,.19f,.145f,.95f,.65f,1.03f},
    {"broad-drooping",14,6,5,.23f,.265f,.68f,1.6f,.94f},
    {"high-crown",11,4,2,.43f,.175f,.80f,1.1f,.90f},
};

void vegBuildTrainingSpruce(std::vector<float>& v,std::vector<unsigned>& idx,int type);
