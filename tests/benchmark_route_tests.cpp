#include "benchmark_route.h"
#include <cstdio>

int main() {
    int failures=0;
    auto check=[&](bool ok,const char* label) {
        if(!ok) { std::fprintf(stderr,"route: %s\n",label); ++failures; }
    };
    for(int stage=0;stage<ROUTE_STAGES;++stage) {
        auto previous=routePose(stage,0);
        bool moved=false,hip=false,scoped=false;
        for(int i=0;i<ROUTE_SAMPLES;++i) {
            auto p=routePose(stage,i);
            check(std::isfinite(p.x+p.z+p.yaw+p.pitch+p.ads),"finite pose");
            check(p.ads>=0 && p.ads<=1,"bounded ADS");
            check(p.pitch>-89 && p.pitch<89,"valid pitch");
            float dx=p.x-previous.x,dz=p.z-previous.z;
            check(std::sqrt(dx*dx+dz*dz)<6.2f/60,"no teleport during measured traversal");
            check(std::fabs(p.yaw-previous.yaw)<190.0f/60,"bounded turn rate");
            check(std::fabs(p.ads-previous.ads)<.11f,"continuous scope transition");
            moved|=dx!=0 || dz!=0; hip|=p.ads==0; scoped|=p.ads==1;
            previous=p;
        }
        check(moved,"each stage traverses the scene");
        check(hip,"includes wide FOV");
        check(scoped==(stage==3),"scope scenario reaches full magnification");
    }
    return failures ? 1 : 0;
}
