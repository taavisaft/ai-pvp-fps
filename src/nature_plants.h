#pragma once
#include <vector>
#include <cmath>

constexpr int NATURE_PLANT_TYPES=6;
struct NaturePlantSample { float x,z; const char* name; };
inline constexpr NaturePlantSample NATURE_PLANTS[] = {
    {8,132,"juniper"}, {12,132,"hazel"}, {16,132,"wild rose"},
    {8,140,"oxeye daisy"}, {12,140,"buttercup"}, {16,140,"fireweed"}
};
// Procedural, two-sided leaf/petal geometry; generated only during GL setup.
void vegBuildNaturePlant(std::vector<float>& v,std::vector<unsigned>& idx,int type);

// Low turf around the catalogue entrance keeps the short flowers visible.
inline float naturePlantClearing(float x,float z) {
    auto fade=[](float v) {
        float t=fmaxf(0,fminf(1,(v-6)/2));
        return 1-t*t*(3-2*t);
    };
    return fade(fabsf(x-12))*fade(fabsf(z-136));
}
