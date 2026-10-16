#include "imported_uzi.h"
#include "imported_uzi_mesh.h"
#include "renderer.h"
#include "texture.h"
#include "map.h"
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>

static bool casingMesh(Mesh& mesh) {
    std::vector<float> v; std::vector<unsigned> idx;
    const int sides=12;
    for(int i=0;i<sides;++i) {
        float a=i*6.2831853f/sides,b=(i+1)*6.2831853f/sides;
        unsigned first=unsigned(v.size()/10);
        for(int end=0;end<2;++end) for(float angle:{a,b}) {
            float x=cosf(angle),y=sinf(angle);
            v.insert(v.end(),{x*.0048f,y*.0048f,end*.019f-.0095f,x,y,0,.48f,.30f,.08f,.65f});
        }
        for(unsigned k:{0u,1u,3u,0u,3u,2u}) idx.push_back(first+k);
        // Closed primer base and dark empty case mouth.
        for(int end=0;end<2;++end) {
            first=unsigned(v.size()/10);float z=end*.019f-.0095f,shade=end ? .32f : .8f;
            for(auto p:{glm::vec2(0),glm::vec2(cosf(a),sinf(a)),glm::vec2(cosf(b),sinf(b))})
                v.insert(v.end(),{p.x*.0048f,p.y*.0048f,z,0,0,end ? 1.0f : -1.0f,.48f*shade,.3f*shade,.08f*shade,.65f});
            if(end) idx.insert(idx.end(),{first,first+1,first+2});
            else idx.insert(idx.end(),{first,first+2,first+1});
        }
    }
    return mesh.create(v.data(),v.size(),idx.data(),idx.size(),true,false,true);
}

bool ImportedUzi::init(const char* base) {
    if(!body[0].create(IMPORTED_UZI_VERTS,sizeof(IMPORTED_UZI_VERTS)/sizeof(float),IMPORTED_UZI_IDX,
        sizeof(IMPORTED_UZI_IDX)/sizeof(unsigned),true,true)) return false;
    if(!body[1].create(IMPORTED_UZI_LOW_VERTS,sizeof(IMPORTED_UZI_LOW_VERTS)/sizeof(float),IMPORTED_UZI_LOW_IDX,
        sizeof(IMPORTED_UZI_LOW_IDX)/sizeof(unsigned),true,true)) return false;
    if(!bolt.create(IMPORTED_UZI_BOLT_VERTS,sizeof(IMPORTED_UZI_BOLT_VERTS)/sizeof(float),IMPORTED_UZI_BOLT_IDX,
        sizeof(IMPORTED_UZI_BOLT_IDX)/sizeof(unsigned),true,true) || !casingMesh(casing)) return false;
    const char* names[] = {"basecolor","normal","surface"};
    for(int i=0;i<3;++i) {
        char p[1024];snprintf(p,sizeof(p),"%stextures/weapons/uzi/%s.png",base,names[i]);
        textures[i]=loadTexture(p);
        if(!textures[i]) { snprintf(p,sizeof(p),"models/textures/weapons/uzi/%s.png",names[i]); textures[i]=loadTexture(p); }
        if(!textures[i]) return false;
    }
    char v[1024],f[1024];snprintf(v,sizeof(v),"%sshaders/basic.vert",base);snprintf(f,sizeof(f),"%sshaders/weapon.frag",base);
    if(!shader.load(v,f) && !shader.load("shaders/basic.vert","shaders/weapon.frag")) return false;
    shader.use();shader.setInt(shader.locDiffuse,0);shader.setInt(shader.locRockMap,2);
    shader.setInt(shader.locDirtMap,3);shader.setInt(shader.locShadowMap,1);
    printf("[uzi] imported: %d near / %d far triangles + 44 bolt; fixed pool %d casings\n",
           body[0].indexCount/3,body[1].indexCount/3,UziEffects::CAPACITY);
    return true;
}

void ImportedUzi::draw(Renderer& r,const glm::mat4& mount,float offset,bool low,bool overlay) {
    bool depth=r.active==&r.depthShader;
    Shader& sh=depth ? *r.active : shader;
    sh.use();
    if(!depth) {
        sh.setMat4(sh.locView,r.curView);sh.setMat4(sh.locProj,r.curProj);
        sh.setMat4(sh.locLightSpace,r.lightSpace);sh.setVec3(sh.locEye,glm::vec3(glm::inverse(r.curView)[3]));
        sh.setVec3(sh.locSunDir,r.sunDir);sh.setVec3(sh.locSunColor,r.sunColor);
        sh.setVec3(sh.locSkyZenith,r.skyZenith);sh.setVec3(sh.locSkyHorizon,r.skyHorizon);
        sh.setVec3(sh.locGroundAmb,r.groundAmbient);sh.setFloat(sh.locExposure,r.exposure);
        sh.setFloat(sh.locSaturation,r.saturation);sh.setFloat(sh.locFogDist,r.fogDist);
        sh.setFloat(sh.locFogHeight,r.fogHeightAmt);
        sh.setInt(sh.locUseShadow,overlay ? 0 : 1);
        const GLenum units[]={GL_TEXTURE0,GL_TEXTURE2,GL_TEXTURE3};
        for(int i=0;i<3;++i) { glActiveTexture(units[i]);glBindTexture(GL_TEXTURE_2D,textures[i]); }
        glActiveTexture(GL_TEXTURE0);sh.setInt(sh.locUseTex,1);
    }
    sh.setMat4(sh.locModel,mount);body[low ? 1 : 0].draw();
    if(!depth) sh.setInt(sh.locUseTex,0);
    sh.setMat4(sh.locModel,glm::translate(mount,{0,0,offset}));bolt.draw();
    r.active->use();
}

void ImportedUzi::drawCasings(Renderer& r,const UziEffects& fx) {
    glDisable(GL_CULL_FACE); // Thin shell rims remain readable from either side.
    for(const auto& c:fx.casings) if(c.life>0) {
        glm::mat4 model=glm::rotate(glm::translate(glm::mat4(1),c.pos),c.angle,c.axis);
        r.drawMeshModel(casing,model,glm::vec3(1));
    }
    glEnable(GL_CULL_FACE);
}
void ImportedUzi::destroy() {
    for(auto& m:body)m.destroy();bolt.destroy();casing.destroy();shader.destroy();
    for(auto& texture:textures) { if(texture)glDeleteTextures(1,&texture);texture=0; }
}

bool useImportedUzi() {
    static const bool legacy=getenv("FPS_LEGACY_UZI")!=nullptr;
    return gMapId==MAP_LOBBY && !legacy;
}
