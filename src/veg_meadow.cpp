#include "vegetation.h"
#include "renderer.h"
#include "map.h"
#include "texture.h"
#include <cstdio>
#include <array>
#include <algorithm>
#include "meadow_density.h"
#include "lobby_growth.h"

void Vegetation::initMeadowAtlas(const char* base) {
    char path[1024];
    snprintf(path,sizeof(path),"%stextures/meadow_atlas.png",base);
    meadowAtlas=loadTextureRGBA(path);
    if(!meadowAtlas) meadowAtlas=loadTextureRGBA("textures/meadow_atlas.png");
}

void Vegetation::prepareMeadow() {
    meadowCards=getenv("FPS_GRASS_RIBBONS")==nullptr && meadowAtlas!=0;
    if(!meadowAtlas) printf("[meadow] atlas missing; using ribbons\n");
    if (getenv("FPS_MEADOW_GPU")) for(auto& timer:meadowTimer) timer.init();
    if (!meadowVbo) {
        std::vector<float> verts;
        std::vector<unsigned> indices;
        if(meadowCards) vegBuildMeadowCards(verts,indices,false);
        else vegBuildMeadow(verts,indices);
        meadowIdx=(GLsizei)indices.size();
        if(meadowCards) vegBuildTrainingMeadow(verts,indices,false);
        trainingIdx=(GLsizei)indices.size()-meadowIdx;
        glGenBuffers(1,&meadowVbo); glGenBuffers(1,&meadowEbo);
        glBindBuffer(GL_ARRAY_BUFFER,meadowVbo);
        glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,meadowEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned),indices.data(),GL_STATIC_DRAW);
    }
    if (!meadowFarVbo) {
        std::vector<float> verts;
        std::vector<unsigned> indices;
        if(meadowCards) vegBuildMeadowCards(verts,indices,true);
        else vegBuildMeadowFar(verts,indices);
        meadowFarIdx=(GLsizei)indices.size();
        if(meadowCards) vegBuildTrainingMeadow(verts,indices,true);
        trainingFarIdx=(GLsizei)indices.size()-meadowFarIdx;
        glGenBuffers(1,&meadowFarVbo); glGenBuffers(1,&meadowFarEbo);
        glBindBuffer(GL_ARRAY_BUFFER,meadowFarVbo);
        glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,meadowFarEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned),indices.data(),GL_STATIC_DRAW);
    }
    meadowEnabled=getenv("FPS_NOMEADOW")==nullptr;
    meadowSide=24;
    meadowRanks.resize(24*24);
    meadowDecoratedRanks.resize(24*24);
    for(int i=0;i<24*24;++i) {
        GrassTile& t=meadowTiles[i];
        t.tx=t.tz=INT_MIN; t.count=0;
        if(!t.vbo) glGenBuffers(1,&t.vbo);
        glBindBuffer(GL_ARRAY_BUFFER,t.vbo);
        glBufferData(GL_ARRAY_BUFFER,900*8*sizeof(float),nullptr,GL_DYNAMIC_DRAW);
        if(!t.vao) t.vao=vegMakeVAO(meadowVbo,meadowEbo,t.vbo);
        t.decoratedCount=0;
        if(!t.decoratedVbo) glGenBuffers(1,&t.decoratedVbo);
        glBindBuffer(GL_ARRAY_BUFFER,t.decoratedVbo);
        glBufferData(GL_ARRAY_BUFFER,MEADOW_DECORATED*8*sizeof(float),nullptr,GL_DYNAMIC_DRAW);
        if(!t.decoratedVao) t.decoratedVao=vegMakeVAO(meadowVbo,meadowEbo,t.decoratedVbo);
        if(!meadowFarVao[i]) meadowFarVao[i]=vegMakeVAO(meadowFarVbo,meadowFarEbo,t.vbo);
    }
    printf("[meadow] 576 reserved tiles, 50 m draw / 55 m preload\n");
}

void Vegetation::drawMeadow(const Renderer& r, const Frustum& fr, const glm::vec3& eye) {
    if(!meadowEnabled) return;
    updateWorldGrass(eye);
    meadowSh.use();
    meadowSh.setMat4(meadowSh.locView, r.curView);
    meadowSh.setMat4(meadowSh.locProj, r.curProj);
    meadowSh.setVec3(meadowSh.locEye, eye);
    meadowSh.setFloat(meadowSh.locTime, r.frameTime);
    meadowSh.setMat4(meadowSh.locLightSpace, r.lightSpace);
    meadowSh.setVec3(meadowSh.locSunDir, r.sunDir);
    meadowSh.setVec3(meadowSh.locSunColor, r.sunColor);
    meadowSh.setVec3(meadowSh.locSkyZenith, r.skyZenith);
    meadowSh.setVec3(meadowSh.locSkyHorizon, r.skyHorizon);
    meadowSh.setVec3(meadowSh.locGroundAmb, r.groundAmbient);
    meadowSh.setFloat(meadowSh.locFogDist, r.fogDist);
    meadowSh.setFloat(meadowSh.locFogHeight, r.fogHeightAmt);
    meadowSh.setFloat(meadowSh.locCloud, r.cloudAmount);
    meadowSh.setFloat(meadowSh.locExposure, r.exposure);
    meadowSh.setFloat(meadowSh.locSaturation, r.saturation);
    meadowSh.setFloat(meadowSh.locHazeCool, r.hazeCool);
    meadowSh.setFloat(locGrassWind,.045f);
    meadowSh.setFloat(locGrassRange,gMapId==MAP_LOBBY ? 65.0f : 60.0f);
    if(meadowCards) {
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D,meadowAtlas);
        glActiveTexture(GL_TEXTURE0);
    }
    MeadowTimer& timer=meadowTimer[0];
    timer.begin();
    for(int iz=0;iz<meadowSide;++iz) for(int ix=0;ix<meadowSide;++ix) {
        const GrassTile& t=meadowTiles[iz*meadowSide+ix];
        if(!t.count || t.tx==INT_MIN) continue;
        glm::vec3 center(t.tx*5+2.5f,(t.minY+t.maxY)*.5f,t.tz*5+2.5f);
        float dx=fmaxf(fabsf(eye.x-center.x)-2.5f,0), dz=fmaxf(fabsf(eye.z-center.z)-2.5f,0);
        float plantHeight=gMapId==MAP_LOBBY ? 1.8f : 1.0f; // include the training seed stalks
        if(!fr.aabbVisible(center,{4.0f,(t.maxY-t.minY)*.5f+plantHeight,4.0f})) continue;
        const float* ranks=meadowRanks[iz*meadowSide+ix].data();
        // Nearest tile point is conservative: per-plant shader thinning handles the
        // rest, identically in visible and shadow passes.
        float distance=sqrtf(dx*dx+dz*dz);
        float density=worldMeadowDensity(distance);
        int count=(int)(std::lower_bound(ranks,ranks+t.count,density)-ranks);
        if(!count) continue;
        if(dx*dx+dz*dz < 32*32) {
            glBindVertexArray(t.vao);
            const bool training=gMapId==MAP_LOBBY && meadowCards;
            glDrawElementsInstanced(GL_TRIANGLES,training ? TRAINING_BLADE_INDICES : meadowIdx,GL_UNSIGNED_INT,
                training ? reinterpret_cast<const void*>(size_t(meadowIdx)*sizeof(unsigned)) : nullptr,count);
            const float* decoratedRanks=meadowDecoratedRanks[iz*meadowSide+ix].data();
            int decorated=(int)(std::lower_bound(decoratedRanks,decoratedRanks+t.decoratedCount,density)-decoratedRanks);
            if(training && decorated) {
                glBindVertexArray(t.decoratedVao);
                glDrawElementsInstanced(GL_TRIANGLES,trainingIdx-TRAINING_BLADE_INDICES,GL_UNSIGNED_INT,
                    reinterpret_cast<const void*>(size_t(meadowIdx+TRAINING_BLADE_INDICES)*sizeof(unsigned)),decorated);
            }
        }
        float farX=fabsf(eye.x-center.x)+2.5f, farZ=fabsf(eye.z-center.z)+2.5f;
        if(farX*farX+farZ*farZ > 18*18) {
            glBindVertexArray(meadowFarVao[iz*meadowSide+ix]);
            const bool training=gMapId==MAP_LOBBY && meadowCards;
            glDrawElementsInstanced(GL_TRIANGLES,training ? trainingFarIdx : meadowFarIdx,GL_UNSIGNED_INT,
                training ? reinterpret_cast<const void*>(size_t(meadowFarIdx)*sizeof(unsigned)) : nullptr,count);
        }
    }
    glBindVertexArray(0);
    timer.end();
}

void Vegetation::destroyMeadow() {
    if(meadowAtlas) glDeleteTextures(1,&meadowAtlas);
    meadowAtlas=0;
    meadowTimer[0].destroy("lit"); meadowTimer[1].destroy("shadow");
    for(auto& t:meadowTiles) {
        if(t.vao) glDeleteVertexArrays(1,&t.vao);
        if(t.vbo) glDeleteBuffers(1,&t.vbo);
        if(t.decoratedVao) glDeleteVertexArrays(1,&t.decoratedVao);
        if(t.decoratedVbo) glDeleteBuffers(1,&t.decoratedVbo);
        t=GrassTile{};
    }
    for(auto& vao:meadowFarVao) { if(vao) glDeleteVertexArrays(1,&vao); vao=0; }
    if(meadowFarVbo) glDeleteBuffers(1,&meadowFarVbo);
    if(meadowFarEbo) glDeleteBuffers(1,&meadowFarEbo);
    meadowFarVbo=meadowFarEbo=0;
    if(meadowVbo) glDeleteBuffers(1,&meadowVbo);
    if(meadowEbo) glDeleteBuffers(1,&meadowEbo);
    meadowVbo=meadowEbo=0;
}
