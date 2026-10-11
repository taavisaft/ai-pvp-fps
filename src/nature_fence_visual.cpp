#include "renderer.h"
#include "map.h"
#include <vector>

namespace {
struct Batch { std::vector<float> v; std::vector<unsigned> idx; };
void box(Batch& b,const glm::vec3& center,const glm::vec3& half,glm::vec3 color,float grain) {
    const glm::vec3 points[]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
                             {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    const int faces[][4]={{4,5,6,7},{1,0,3,2},{0,4,7,3},{5,1,2,6},{3,7,6,2},{0,1,5,4}};
    const glm::vec3 normals[]={{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    for(int f=0;f<6;++f) {
        unsigned first=unsigned(b.v.size()/10);
        glm::vec3 c=color*(f==5 ? .65f : 1);
        for(int j=0;j<4;++j) {
            glm::vec3 p=center+points[faces[f][j]]*half,n=normals[f];
            b.v.insert(b.v.end(),{p.x,p.y,p.z,n.x,n.y,n.z,c.x,c.y,c.z,grain});
        }
        for(unsigned j:{0u,1u,2u,2u,3u,0u}) b.idx.push_back(first+j);
    }
}
void timber(const NatureFencePart& p,void* context) {
    Batch& b=*static_cast<Batch*>(context);
    float fade=.80f+.20f*sinf(p.seed*2.39996f);
    box(b,p.center,p.half,glm::vec3(.068f,.064f,.048f)*fade,float(p.grain));
    if(!p.post) return;
    // Chipped, uneven cap and rust-dark fasteners on both faces of each post.
    float base=p.center.y-p.half.y+.16f;
    for(float level:{.48f,1.08f}) for(float sign:{-1.0f,1.0f}) {
        box(b,{p.center.x+sign*.108f,base+level,p.center.z},{.004f,.018f,.018f},{.025f,.017f,.009f},.18f);
        box(b,{p.center.x,base+level,p.center.z+sign*.108f},{.018f,.018f,.004f},{.025f,.017f,.009f},.18f);
    }
}
}

bool Renderer::initNatureFence() {
    Batch b;
    b.v.reserve(240000); b.idx.reserve(40000);
    // Renderer setup runs before the active lobby is chosen; sample explicitly.
    visitNatureFence(timber,&b,trainingHeight);
    return natureFence.create(b.v.data(),b.v.size(),b.idx.data(),b.idx.size(),true,false,true);
}

void Renderer::drawNatureFence(const Frustum& fr) {
    if(gMapId!=MAP_LOBBY || !fr.aabbVisible({37,16,120},{34,30,29})) return;
    drawMeshModel(natureFence,glm::mat4(1),glm::vec3(1));
}
