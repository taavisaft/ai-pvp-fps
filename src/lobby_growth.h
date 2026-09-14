#pragma once
#include <cmath>
#include <cstdint>

// Visual-only training-area growth field; exact counterpart in lobby_growth.glsl.
inline float lobbyGrowthNoise(float x, float z) {
    auto hash=[](int x, int z) {
        uint32_t n=uint32_t(x)*374761393u+uint32_t(z)*668265263u;
        n=(n^(n>>13))*1274126177u; n^=n>>16;
        return float(n&0x7fffffffu)/2147483647.0f;
    };
    int ix=(int)floorf(x), iz=(int)floorf(z);
    float u=x-ix, v=z-iz; u=u*u*(3-2*u); v=v*v*(3-2*v);
    float a=hash(ix,iz), b=hash(ix+1,iz), c=hash(ix,iz+1), d=hash(ix+1,iz+1);
    return (a+(b-a)*u)*(1-v)+(c+(d-c)*u)*v;
}
inline float lobbyGrowth(float x, float z) {
    float warp=lobbyGrowthNoise(x*.025f+17,z*.025f-9)*1.5f;
    float g=.75f*lobbyGrowthNoise(x*.075f+warp,z*.075f-warp)
           +.25f*lobbyGrowthNoise(x*.22f+5,z*.22f);
    float t=fmaxf(0,fminf(1,(g-.30f)/.38f));
    return t*t*(3-2*t);
}
inline float lobbyGrowthDensity(float growth) { return .015f+.985f*growth*growth; }
