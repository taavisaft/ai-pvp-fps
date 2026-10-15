#include "satellite_map.h"
#include "map.h"
#include "tree_scatter.h"
#include "training_woodland.h"
#include "forest_floor.h"
#include "lobby_growth.h"
#include <algorithm>

namespace {
float smooth(float a,float b,float x) { return lobbySmooth(a,b,x); }
struct Image {
    uint8_t* rgb;
    int side;
    float half,step;
    float world(int p) const { return -half+(p+.5f)*step; }
    float pixel(float w) const { return (w+half)/step-.5f; }
    void blend(int x,int z,glm::vec3 c,float opacity=1) {
        if(x<0 || z<0 || x>=side || z>=side) return;
        uint8_t* p=rgb+(z*side+x)*3;
        for(int k=0;k<3;++k)
            p[k]=uint8_t(glm::clamp(p[k]*(1-opacity)+c[k]*255*opacity,0.0f,255.0f));
    }
    // Lobed, softly shaded crowns rather than symbols. Shape and size follow
    // the rendered species; every stamp comes from one real tree instance.
    void crown(const TreeInstance& t,int type,bool shadow) {
        float radius=t.scale*trainingTreeWidth(type)*.45f;
        float reach=shadow ? t.scale*.48f : 0;
        float cx=pixel(t.x+reach*.7f), cz=pixel(t.z-reach*.7f);
        float r=std::max(.6f,radius/step), bound=r*1.18f;
        glm::vec3 color=type<4 ? glm::vec3(.12f,.20f,.105f)
                             : type==5 ? glm::vec3(.29f,.37f,.16f) : glm::vec3(.23f,.31f,.13f);
        for(int z=std::max(0,int(floorf(cz-bound)));z<=std::min(side-1,int(ceilf(cz+bound)));++z)
            for(int x=std::max(0,int(floorf(cx-bound)));x<=std::min(side-1,int(ceilf(cx+bound)));++x) {
                float dx=(x-cx)/r,dz=(z-cz)/r;
                float angle=atan2f(dz,dx)+t.yaw;
                float edge=1+.10f*sinf(angle*(type<4 ? 7 : 5));
                float d=sqrtf(dx*dx+dz*dz)/edge;
                float alpha=1-smooth(.80f,1.05f,d);
                if(alpha<=0) continue;
                if(shadow) { blend(x,z,{.06f,.085f,.055f},alpha*.34f); continue; }
                float light=.72f+.38f*sqrtf(std::max(0.0f,1-d*d))-.16f*dx+.16f*dz;
                float grain=.93f+.14f*terrHash(x,z);
                blend(x,z,color*(light*grain*t.tint),alpha);
            }
    }
    void box(const Box& b,bool shadow) {
        float offset=shadow ? std::max(.5f,b.half.y)*.8f : 0;
        int x0=int(floorf(pixel(b.center.x-b.half.x+offset)));
        int x1=int(ceilf(pixel(b.center.x+b.half.x+offset)));
        int z0=int(floorf(pixel(b.center.z-b.half.z-offset)));
        int z1=int(ceilf(pixel(b.center.z+b.half.z-offset)));
        for(int z=std::max(0,z0);z<=std::min(side-1,z1);++z)
            for(int x=std::max(0,x0);x<=std::min(side-1,x1);++x) {
                bool edge=x==x0 || x==x1 || z==z0 || z==z1;
                glm::vec3 c=shadow ? glm::vec3(.06f,.07f,.05f)
                    : glm::vec3(.49f,.47f,.40f)*(edge ? .7f : 1.0f);
                blend(x,z,c,shadow ? .45f : 1.0f);
            }
    }
};
}

bool bakeSatelliteMap(uint8_t* rgb,float* heights,int side,float half,const Box* boxes,int count) {
    if(!rgb || !heights || side<2 || side>4096 || !std::isfinite(half) || half<=0 ||
       count<0 || (count && !boxes)) return false;
    Image image{rgb,side,half,2*half/side};
    for(int z=0;z<side;++z) for(int x=0;x<side;++x)
        heights[z*side+x]=terrainHeight(image.world(x),image.world(z));
    const glm::vec3 sun=glm::normalize(glm::vec3(-.6f,1,.6f));
    for(int z=0;z<side;++z) for(int x=0;x<side;++x) {
        float wx=image.world(x),wz=image.world(z),h=heights[z*side+x];
        float dx=(heights[z*side+std::min(x+1,side-1)]-heights[z*side+std::max(x-1,0)])/(2*image.step);
        float dz=(heights[std::min(z+1,side-1)*side+x]-heights[std::max(z-1,0)*side+x])/(2*image.step);
        glm::vec3 normal=glm::normalize(glm::vec3(-dx,1,-dz));
        float grain=terrValueNoise(wx*.18f,wz*.18f);
        float growth=lobbyGrowth(wx,wz);
        glm::vec3 meadow=glm::mix(glm::vec3(.40f,.40f,.23f),glm::vec3(.29f,.38f,.16f),growth);
        glm::vec3 floor=glm::mix(glm::vec3(.26f,.24f,.16f),glm::vec3(.23f,.29f,.14f),grain);
        glm::vec3 color=glm::mix(meadow,floor,forestFloorCover(wx,wz,h));
        float rock=smooth(.30f,.55f,(1-normal.y)*6);
        if(gMapId==MAP_PALDISKI) {
            rock=std::max(rock,smooth(115,175,h));
            float beach=(1-smooth(.25f,.95f,h))*(1-smooth(-600,-350,wx));
            color=glm::mix(color,glm::vec3(.53f,.49f,.34f),beach);
        } else {
            color=glm::mix(color,glm::vec3(.47f,.43f,.32f),lobbyWear(wx,wz));
        }
        color=glm::mix(color,glm::vec3(.49f,.48f,.42f),rock);
        // Broad relief plus local slope shading makes ridges/valleys readable
        // without invented contours or changing the actual terrain heights.
        float relief=.88f+.20f*smooth(0,180,h);
        color*=relief*(.60f+.60f*std::max(0.0f,glm::dot(normal,sun)))*(.94f+.12f*grain);
        bool water=gMapId==MAP_PALDISKI ? h<SEA_LEVEL
            : trainingPondRadius(wx,wz)<1.8f && h<TRAINING_POND_Y;
        if(water) {
            float level=gMapId==MAP_PALDISKI ? SEA_LEVEL : TRAINING_POND_Y;
            color=glm::mix(glm::vec3(.18f,.32f,.31f),glm::vec3(.08f,.19f,.24f),smooth(0,12,level-h));
        }
        image.blend(x,z,color);
    }
    for(const auto& tree:gTrees) {
        int type=gMapId==MAP_LOBBY ? trainingTreeType(tree.x,tree.z)
            : taigaTreeType(tree.x,tree.z,pineForestBiome(tree.x,tree.z));
        image.crown(tree,type,true);
    }
    for(int i=0;i<count;++i) image.box(boxes[i],true);
    for(const auto& tree:gTrees) {
        int type=gMapId==MAP_LOBBY ? trainingTreeType(tree.x,tree.z)
            : taigaTreeType(tree.x,tree.z,pineForestBiome(tree.x,tree.z));
        image.crown(tree,type,false);
    }
    for(int i=0;i<count;++i) image.box(boxes[i],false);
    return true;
}
