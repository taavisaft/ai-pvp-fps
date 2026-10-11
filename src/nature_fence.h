#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <initializer_list>

constexpr float NATURE_FENCE_WEST=4, NATURE_FENCE_EAST=70;
constexpr float NATURE_FENCE_SOUTH=92, NATURE_FENCE_NORTH=148;
constexpr float NATURE_FENCE_ENTRY_START=132, NATURE_FENCE_ENTRY_END=140;
inline int gNatureFenceFirst=0,gNatureFenceCount=0;
struct NatureFencePart { glm::vec3 center,half; int grain,seed; bool post; };
using NatureFenceVisitor=void (*)(const NatureFencePart&,void*);

// Shared stepped timber follows the terrain. Every visible timber box is solid.
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
        int pieces=int(ceilf(length/.75f));
        for(int i=0;i<pieces;++i) {
            float step=length/pieces;
            glm::vec3 p=glm::vec3(x,0,z)+direction*(step*(i+.5f));
            float base=height(p.x,p.z);
            for(float level:{.48f,1.08f}) {
                glm::vec3 half=direction.x!=0 ? glm::vec3(step*.5f+.006f,.065f,.065f)
                                                           : glm::vec3(.065f,.065f,step*.5f+.006f);
                visit({{p.x,base+level,p.z},half,direction.x!=0 ? 3 : 4,seed++,false},context);
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
