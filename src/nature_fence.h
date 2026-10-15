#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <initializer_list>

constexpr float NATURE_FENCE_WEST=4, NATURE_FENCE_EAST=70;
constexpr float NATURE_FENCE_SOUTH=92, NATURE_FENCE_NORTH=148;
constexpr float NATURE_FENCE_ENTRY_START=132, NATURE_FENCE_ENTRY_END=140;
inline int gNatureFenceFirst=0,gNatureFenceCount=0;
struct NatureFencePart { glm::vec3 center,half; int grain,seed; bool post; glm::vec3 axis{0}; };
using NatureFenceVisitor=void (*)(const NatureFencePart&,void*);

// Continuous round rails connect adjacent posts and slope with the terrain.
// grain: 2=vertical post, 3=east/west rail, 4=north/south rail.
inline void visitNatureFence(NatureFenceVisitor visit,void* context,float (*height)(float,float)) {
    int seed=0;
    auto side=[&](float x,float z,float ex,float ez) {
        float length=hypotf(ex-x,ez-z);
        int bays=int(ceilf(length/3));
        glm::vec3 direction((ex-x)/length,0,(ez-z)/length);
        for(int i=0;i<=bays;++i) {
            glm::vec3 p=glm::vec3(x,0,z)+direction*(length*i/bays);
            float base=height(p.x,p.z),top=base+1.42f+.035f*sinf(seed*5.3f);
            visit({{p.x,(base-.16f+top)*.5f,p.z},{.105f,(top-base+.16f)*.5f,.105f},2,seed++,true},context);
            visit({{p.x+.012f,top-.012f,p.z-.008f},{.085f,.025f,.080f},2,seed++,false},context);
        }
        for(int i=0;i<bays;++i) {
            glm::vec3 start=glm::vec3(x,0,z)+direction*(length*i/bays);
            glm::vec3 end=glm::vec3(x,0,z)+direction*(length*(i+1)/bays);
            start.y=height(start.x,start.z); end.y=height(end.x,end.z);
            for(float level:{.48f,1.08f}) {
                glm::vec3 offset(0,level,0);
                visit({(start+end)*.5f+offset,{.075f,.075f,.075f},
                       direction.x!=0 ? 3 : 4,seed++,false,(end-start)*.5f},context);
            }
        }
    };
    side(4,92,70,92); side(70,92,70,148); side(70,148,4,148);
    side(4,92,4,132); side(4,140,4,148); // Eight-metre walk-in opening facing spawn.
}
inline bool natureFenceClearance(float x,float z,float margin) {
    bool across=x>4-margin && x<70+margin;
    bool along=z>92-margin && z<148+margin;
    return (across && (fabsf(z-92)<margin || fabsf(z-148)<margin)) ||
           (along && (fabsf(x-70)<margin ||
            (fabsf(x-4)<margin && (z<132+margin || z>140-margin))));
}

constexpr int NATURE_LOG_SIDES=12;
inline NatureFencePart gNatureLogs[200];
inline int gNatureLogCount=0;
inline bool gNatureRailBox[4096]{};
inline void visitNatureFenceColliders(NatureFenceVisitor visit,void* context,float (*height)(float,float)) {
    struct Expand { NatureFenceVisitor visit; void* context; } e{visit,context};
    visitNatureFence([](const NatureFencePart& p,void* ctx) {
        auto& e=*static_cast<Expand*>(ctx);
        if(p.grain<3) { e.visit(p,e.context); return; }
        int pieces=int(ceilf(glm::length(glm::vec2(p.axis.x,p.axis.z))*2/.75f));
        for(int i=0;i<pieces;++i) {
            NatureFencePart slice=p;
            slice.center=p.center+p.axis*((i+.5f)*2/pieces-1);
            slice.half=glm::abs(p.axis)/float(pieces)+glm::vec3(.075f);
            e.visit(slice,e.context);
        }
    },&e,height);
}
// Exact convex round-log prism, also used by the rendered twelve-sided mesh.
inline bool segmentNatureLog(const NatureFencePart& log,glm::vec3 start,glm::vec3 end,
                             float& fraction,glm::vec3& normal) {
    glm::vec3 axis=glm::normalize(log.axis);
    glm::vec3 side=glm::normalize(glm::cross(axis,glm::vec3(0,1,0)));
    glm::vec3 up=glm::cross(side,axis),p=start-log.center,d=end-start;
    normal=up;
    float enter=0,leave=1;
    auto clip=[&](glm::vec3 n,float limit) {
        float distance=glm::dot(p,n)-limit,speed=glm::dot(d,n);
        if(fabsf(speed)<1e-8f) return distance<=0;
        float t=-distance/speed;
        if(speed<0) { if(t>=enter) { enter=t;normal=n; } }
        else leave=fminf(leave,t);
        return enter<=leave;
    };
    float length=glm::length(log.axis);
    if(!clip(axis,length) || !clip(-axis,length)) return false;
    for(int i=0;i<NATURE_LOG_SIDES;++i) {
        float a=(i+.5f)*6.2831853f/NATURE_LOG_SIDES;
        if(!clip(side*cosf(a)+up*sinf(a),.075f*cosf(3.14159265f/NATURE_LOG_SIDES))) return false;
    }
    fraction=enter; return true;
}
