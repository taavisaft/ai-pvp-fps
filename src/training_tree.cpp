#include "vegetation.h"
#include "texture.h"
#include <algorithm>
#include <array>
#include <cstdio>

bool Vegetation::initTrainingTrees(const char* base) {
    locTrainingTree=glGetUniformLocation(vegSh.program,"trainingTree");
    locTrainingTreeD=glGetUniformLocation(vegDepthSh.program,"trainingTree");
    char path[768]; snprintf(path,sizeof(path),"%stextures/training_spruce_branch.png",base);
    trainingBranchTex=loadTextureRGBA(path);
    if(!trainingBranchTex) trainingBranchTex=loadTextureRGBA("textures/training_spruce_branch.png");
    if(!trainingBranchTex) return false;
    glBindTexture(GL_TEXTURE_2D,trainingBranchTex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,5);
    const GLuint streams[]={streamL0,streamL1,streamShadow};
    for(int type=0;type<TRAINING_SPRUCE_TYPES;++type) {
        auto& mesh=trainingSpruce[type];
        std::vector<float> v; std::vector<unsigned> idx;
        vegBuildTrainingSpruce(v,idx,type);
        mesh.count=(GLsizei)idx.size();
        glGenBuffers(1,&mesh.vbo); glGenBuffers(1,&mesh.ebo);
        glBindBuffer(GL_ARRAY_BUFFER,mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned),idx.data(),GL_STATIC_DRAW);
        for(int pass=0;pass<3;++pass) mesh.vao[pass]=vegMakeVAO(mesh.vbo,mesh.ebo,streams[pass]);
        printf("[veg] training spruce %s: %d triangles\n",TRAINING_SPRUCE_PROFILES[type].name,mesh.count/3);
        if(!vegBakeImpostor(*this,256,512,type)) return false;
    }
    return true;
}

// Shared classification for lit mesh, shadows, and impostors. Fixed staging
// handles arbitrarily many trees in chunks, with no allocation during drawing.
void Vegetation::drawTrainingTreeStream(const std::vector<float>& buf,int pass) {
    const GLuint streams[]={streamL0,streamL1,streamShadow,streamImp};
    std::array<float,128*8> staging;
    for(int type=0;type<TRAINING_SPRUCE_TYPES;++type) {
        const auto& mesh=trainingSpruce[type];
        int count=0;
        auto flush=[&]() {
            if(!count) return;
            glBindBuffer(GL_ARRAY_BUFFER,streams[pass]);
            glBufferData(GL_ARRAY_BUFFER,count*8*sizeof(float),staging.data(),GL_STREAM_DRAW);
            if(pass==3) {
                glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D,mesh.impostor);
                glActiveTexture(GL_TEXTURE0); glBindVertexArray(vaoImp);
                glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,count);
            } else {
                glBindVertexArray(mesh.vao[pass]);
                glDrawElementsInstanced(GL_TRIANGLES,mesh.count,GL_UNSIGNED_INT,nullptr,count);
            }
            count=0;
        };
        for(size_t i=0;i+7<buf.size();i+=8) {
            if(trainingSpruceType(buf[i],buf[i+2])!=type) continue;
            std::copy_n(buf.data()+i,8,staging.data()+count*8);
            if(++count==128) flush();
        }
        flush();
    }
    glBindVertexArray(0);
}

void Vegetation::logTrainingTreeMix() const {
    int counts[TRAINING_SPRUCE_TYPES]{};
    for(const auto& t:trees) ++counts[trainingSpruceType(t.pos.x,t.pos.z)];
    printf("[veg] spruce mix: full=%d narrow=%d broad-drooping=%d high-crown=%d\n",
           counts[0],counts[1],counts[2],counts[3]);
}

void Vegetation::destroyTrainingTrees() {
    for(auto& mesh:trainingSpruce) {
        for(auto& vao:mesh.vao) if(vao) glDeleteVertexArrays(1,&vao);
        if(mesh.vbo) glDeleteBuffers(1,&mesh.vbo);
        if(mesh.ebo) glDeleteBuffers(1,&mesh.ebo);
        if(mesh.impostor) glDeleteTextures(1,&mesh.impostor);
        mesh=SpruceMesh{};
    }
    if(trainingBranchTex) glDeleteTextures(1,&trainingBranchTex);
    trainingBranchTex=0;
}
