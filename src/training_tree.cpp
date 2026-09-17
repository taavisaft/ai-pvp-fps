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
    snprintf(path,sizeof(path),"%stextures/training_broadleaf_atlas.png",base);
    trainingBroadleafTex=loadTextureRGBA(path);
    if(!trainingBroadleafTex) trainingBroadleafTex=loadTextureRGBA("textures/training_broadleaf_atlas.png");
    if(!trainingBroadleafTex) return false;
    glBindTexture(GL_TEXTURE_2D,trainingBroadleafTex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,5);
    auto upload=[](GLuint& vbo,GLuint& ebo,const std::vector<float>& v,const std::vector<unsigned>& idx) {
        glGenBuffers(1,&vbo); glGenBuffers(1,&ebo);
        glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned),idx.data(),GL_STATIC_DRAW);
    };
    for(int type=0;type<TRAINING_TREE_TYPES;++type) {
        auto& mesh=trainingSpruce[type];
        for(int low=0;low<2;++low) {
            std::vector<float> v; std::vector<unsigned> idx;
            if(type<4) vegBuildTrainingSpruce(v,idx,type,low);
            else vegBuildBroadleaf(v,idx,type-4,low);
            if(low) { upload(mesh.lowVbo,mesh.lowEbo,v,idx); mesh.lowCount=(GLsizei)idx.size(); }
            else { upload(mesh.vbo,mesh.ebo,v,idx); mesh.count=(GLsizei)idx.size(); }
        }
        mesh.vao[TRAINING_L0]=vegMakeVAO(mesh.vbo,mesh.ebo,streamL0);
        mesh.vao[TRAINING_L1]=vegMakeVAO(mesh.lowVbo,mesh.lowEbo,streamL1);
        mesh.vao[TRAINING_SHADOW]=vegMakeVAO(mesh.vbo,mesh.ebo,streamShadow);
        mesh.vao[TRAINING_SHADOW_LOW]=vegMakeVAO(mesh.lowVbo,mesh.lowEbo,streamShadow);
        printf("[veg] training tree %s: %d triangles, low %d\n",TRAINING_TREE_NAMES[type],
               mesh.count/3,mesh.lowCount/3);
        if(!vegBakeImpostor(*this,256,512,type)) return false;
    }
    return true;
}

void Vegetation::reserveTrainingStaging() {
    size_t counts[TRAINING_TREE_TYPES]{};
    for(const auto& t:trees) ++counts[t.type];
    for(int type=0;type<TRAINING_TREE_TYPES;++type)
        for(SpeciesStaging* staging:{&speciesL0,&speciesL1,&speciesImp,&speciesShadow,&speciesShadowLow,&speciesShrub})
            (*staging)[type].reserve(counts[type]*8);
}

void Vegetation::drawTrainingTreeStream(const SpeciesStaging& staging,TrainingPass pass) {
    const GLuint streams[]={streamL0,streamL1,streamShadow,streamShadow,streamImp};
    const bool shadow=pass==TRAINING_SHADOW || pass==TRAINING_SHADOW_LOW;
    const bool low=pass==TRAINING_L1 || pass==TRAINING_SHADOW_LOW;
    for(int type=0;type<TRAINING_TREE_TYPES;++type) {
        const std::vector<float>& buf=staging[type];
        if(buf.empty()) continue;
        const auto& mesh=trainingSpruce[type];
        GLsizei count=(GLsizei)(buf.size()/8);
        glBindBuffer(GL_ARRAY_BUFFER,streams[pass]);
        glBufferData(GL_ARRAY_BUFFER,buf.size()*sizeof(float),buf.data(),GL_STREAM_DRAW);
        if(pass==TRAINING_IMPOSTOR) {
            glUniform2f(locImpSize,trainingTreeWidth(type),1.10f);
            glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D,mesh.impostor);
            glActiveTexture(GL_TEXTURE0); glBindVertexArray(vaoImp);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,count);
            continue;
        }
        glUniform1i(shadow ? locTrainingTreeD : locTrainingTree,type<4 ? 1 : type-2);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D,type<4 ? trainingBranchTex : trainingBroadleafTex);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(mesh.vao[pass]);
        glDrawElementsInstanced(GL_TRIANGLES,low ? mesh.lowCount : mesh.count,GL_UNSIGNED_INT,nullptr,count);
    }
    glBindVertexArray(0);
}

void Vegetation::logTrainingTreeMix() const {
    int counts[TRAINING_TREE_TYPES]{};
    for(const auto& t:trees) ++counts[t.type];
    for(int type=0;type<TRAINING_TREE_TYPES;++type)
        printf("[veg] woodland %s: %d trees\n",TRAINING_TREE_NAMES[type],counts[type]);
}

void Vegetation::destroyTrainingTrees() {
    for(auto& mesh:trainingSpruce) {
        for(auto& vao:mesh.vao) if(vao) glDeleteVertexArrays(1,&vao);
        for(GLuint buffer:{mesh.vbo,mesh.ebo,mesh.lowVbo,mesh.lowEbo}) if(buffer) glDeleteBuffers(1,&buffer);
        if(mesh.impostor) glDeleteTextures(1,&mesh.impostor);
        mesh=SpruceMesh{};
    }
    if(trainingBranchTex) glDeleteTextures(1,&trainingBranchTex);
    if(trainingBroadleafTex) glDeleteTextures(1,&trainingBroadleafTex);
    trainingBranchTex=trainingBroadleafTex=0;
}
