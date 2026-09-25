#pragma once
#include <cmath>

constexpr int ROUTE_STAGES=4, ROUTE_WARMUP=180, ROUTE_SAMPLES=600;
struct RoutePose { float x,z,yaw,pitch,ads; };
inline const char* routeStageName(int stage) {
    static const char* names[]={"pond_approach","pond_turns","forest_walk","scope_cycles"};
    return names[stage];
}
// Fixed 60 Hz route samples give every run identical viewpoints and streaming
// steps, independently of measured FPS. This is camera traversal, not physics.
inline RoutePose routePose(int stage,int sample) {
    float t=float(sample)/60.0f;
    if(stage==0) return {-46+2.2f*t,100+4.6f*t,48,-6,0};
    if(stage==1) return {-24+.8f*t,146,90+120*sinf(t*1.5f),-8,0};
    if(stage==2) return {t,550+6*t,90+20*sinf(t*.7f),-6,0};
    float phase=fmodf(t,5.0f);
    float ads=fminf(1.0f,fmaxf(0.0f,fminf((phase-1)*4,(4-phase)*4)));
    ads=ads*ads*(3-2*ads);
    return {10+1.2f*t,610,90+8*sinf(t*.8f),-4,ads};
}
