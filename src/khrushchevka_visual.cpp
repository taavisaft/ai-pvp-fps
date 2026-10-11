#include "khrushchevka_visual.h"
#include "renderer.h"
#include "renderer_bind.h"
#include "texture.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace {
struct Batch { std::vector<float> vertices; std::vector<unsigned> indices; };
struct Export {
    Batch batches[KHRUSH_SECTIONS][2];
    bool far;
};
void append(const KhrushPart& p,void* context) {
    Export& e=*static_cast<Export*>(context);
    if(e.far && p.detail) return;
    Batch& b=e.batches[p.section][p.concrete ? 0 : 1];
    const glm::vec3 points[8]={
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
        {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    const int faces[6][4]={{4,5,6,7},{1,0,3,2},{0,4,7,3},{5,1,2,6},{3,7,6,2},{0,1,5,4}};
    const glm::vec3 normals[6]={{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    for(int f=0;f<6;++f) {
        unsigned start=static_cast<unsigned>(b.vertices.size()/10);
        for(int corner=0;corner<4;++corner) {
            glm::vec3 pos=p.center+points[faces[f][corner]]*p.half;
            const glm::vec3& n=normals[f];
            // Subtle darker underside, independent of dynamic sun lighting.
            glm::vec3 c=p.color*(n.y<0 ? .80f : 1.0f);
            float spec=p.concrete ? .03f : .20f;
            b.vertices.insert(b.vertices.end(),{pos.x,pos.y,pos.z,n.x,n.y,n.z,c.x,c.y,c.z,spec});
        }
        for(unsigned index:{0u,1u,2u,2u,3u,0u}) b.indices.push_back(start+index);
    }
}
}

bool KhrushVisual::init(const char* base) {
    char path[1024];
    snprintf(path,sizeof(path),"%stextures/environment/khrushchevka/Concrete034_1K-JPG_Color.jpg",base);
    concreteTexture=loadTexture(path);
    if(!concreteTexture) concreteTexture=loadTexture("models/textures/environment/khrushchevka/Concrete034_1K-JPG_Color.jpg");
    if(!concreteTexture) concreteTexture=makeConcreteTexture();
    if(!concreteTexture) return false;
    for(int lod=0;lod<2;++lod) {
        Export e{}; e.far=lod==1;
        for(auto& section:e.batches) for(auto& b:section) {
            b.vertices.reserve(160000); b.indices.reserve(24000);
        }
        visitKhrushchevka(append,&e);
        size_t triangles=0;
        for(int section=0;section<KHRUSH_SECTIONS;++section) for(int group=0;group<2;++group) {
            const Batch& b=e.batches[section][group];
            if(!meshes[section][group][lod].create(b.vertices.data(),b.vertices.size(),
                 b.indices.data(),b.indices.size(),true,false,true)) return false;
            triangles+=b.indices.size()/3;
        }
        printf("[building] %s: %zu triangles, eight sections\n",lod ? "far" : "near",triangles);
    }
    return true;
}

void KhrushVisual::draw(Renderer& r,const Frustum& fr,const glm::vec3& eye) const {
    if(gMapId!=MAP_LOBBY) return;
    static const bool hidden=getenv("FPS_NOBUILDING")!=nullptr; // render-cost isolation
    if(hidden) return;
    Shader& sh=*r.active;
    glm::vec3 origin(KHRUSH_X,KHRUSH_Y,KHRUSH_Z);
    sh.setMat4(sh.locModel,glm::translate(glm::mat4(1),origin));
    sh.setVec3(sh.locColor,glm::vec3(1));
    sh.setInt(sh.locHasNormal,1);
    sh.setInt(sh.locAuthoredMaterial,1);
    for(int section=0;section<KHRUSH_SECTIONS;++section) {
        glm::vec3 center=origin+glm::vec3(-KHRUSH_LENGTH*.5f+(section+.5f)*KHRUSH_SECTION,7.4f,0);
        const glm::vec3 half(7.5f,8.2f,7.6f);
        if(!fr.aabbVisible(center,half)) continue;
        // Distance to the section footprint, so nearby corner details stay visible.
        glm::vec3 delta=glm::max(glm::abs(eye-center)-half,glm::vec3(0));
        int lod=glm::dot(delta,delta)>100*100 ? 1 : 0;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,concreteTexture);
        sh.setInt(sh.locUseTex,1);
        sh.setFloat(sh.locTile,1.1f);
        sh.setVec3(sh.locTint,glm::vec3(1));
        meshes[section][0][lod].draw();
        sh.setInt(sh.locUseTex,0);
        meshes[section][1][lod].draw();
    }
    sh.setInt(sh.locUseTex,0);
    sh.setInt(sh.locAuthoredMaterial,0);
    sh.setInt(sh.locHasNormal,0);
}

void KhrushVisual::destroy() {
    for(auto& section:meshes) for(auto& group:section) for(auto& mesh:group) mesh.destroy();
    if(concreteTexture) glDeleteTextures(1,&concreteTexture);
    concreteTexture=0;
}
