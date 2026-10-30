#include "vegetation.h"
#include "texture.h"
#include "map.h"
#include <algorithm>
#include <array>
#include <cstdio>

static bool importedTree(const char* base,int type,int lod,std::vector<float>& v,std::vector<unsigned>& idx) {
    const char* dir=trainingImportedPath(type);
    char path[1024]; snprintf(path,sizeof(path),"%stextures/%s/lod%d.bin",base,dir,lod);
    FILE* f=fopen(path,"rb");
    if(!f) { snprintf(path,sizeof(path),"models/textures/%s/lod%d.bin",dir,lod); f=fopen(path,"rb"); }
    if(!f) return false;
    uint32_t counts[2];
    if(fread(counts,sizeof(uint32_t),2,f)!=2 || counts[0]%12 || counts[0]>12000000 || counts[1]>3000000) { fclose(f); return false; }
    v.resize(counts[0]); idx.resize(counts[1]);
    bool ok=fread(v.data(),sizeof(float),v.size(),f)==v.size() && fread(idx.data(),sizeof(unsigned),idx.size(),f)==idx.size();
    fclose(f);
    if(ok) for(unsigned i:idx) if(i>=v.size()/12) return false;
    return ok;
}

bool Vegetation::initTrainingTrees(const char* base) {
    for(int i=0;i<3;++i) {
        char atlas[1024]; const char* dir=i==2 ? "elm" : trainingImportedPath(8+i);
        snprintf(atlas,sizeof(atlas),"%stextures/%s/atlas.png",base,dir);
        importedTreeTex[i]=loadTextureRGBA(atlas);
        if(!importedTreeTex[i]) {
            snprintf(atlas,sizeof(atlas),"models/textures/%s/atlas.png",dir);
            importedTreeTex[i]=loadTextureRGBA(atlas);
        }
        if(!importedTreeTex[i]) return false;
        glBindTexture(GL_TEXTURE_2D,importedTreeTex[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,2);
    }
    locTrainingTree=glGetUniformLocation(vegSh.program,"trainingTree");
    locTrainingTreeD=glGetUniformLocation(vegDepthSh.program,"trainingTree");
    char path[768]; snprintf(path,sizeof(path),"%stextures/spruce_bough_v2.png",base);
    trainingBranchTex=loadTextureRGBA(path);
    if(!trainingBranchTex) trainingBranchTex=loadTextureRGBA("textures/spruce_bough_v2.png");
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
            if(type>=8) { if(!importedTree(base,type,low,v,idx)) return false; }
            else if(trainingConifer(type)) vegBuildTrainingSpruce(v,idx,type==7 ? 4 : type,low);
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
        if(!trainingConifer(type) && !vegBakeImpostor(*this,256,512,type,true)) return false;
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
        if (!shadow) glUniform1i(pass == TRAINING_IMPOSTOR ? locImpStableBroadleaf : locStableBroadleaf,
                                 trainingConifer(type) ? 0 : 1);
        glBindBuffer(GL_ARRAY_BUFFER,streams[pass]);
        glBufferData(GL_ARRAY_BUFFER,buf.size()*sizeof(float),buf.data(),GL_STREAM_DRAW);
        if(pass==TRAINING_IMPOSTOR) {
            glUniform2f(locImpSize,trainingTreeWidth(type),1.10f);
            glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D,trainingConifer(type) ? mesh.impostor : mesh.lobbyImpostor);
            glActiveTexture(GL_TEXTURE0); glBindVertexArray(vaoImp);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,count);
            continue;
        }
        glUniform1i(shadow ? locTrainingTreeD : locTrainingTree,trainingTreeShader(type));
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D,type>=8 ? importedTreeTex[trainingImportedTextureSlot(type)] : trainingConifer(type) ? trainingBranchTex : trainingBroadleafTex);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(mesh.vao[pass]);
        glDrawElementsInstanced(GL_TRIANGLES,low ? mesh.lowCount : mesh.count,GL_UNSIGNED_INT,nullptr,count);
    }
    if (!shadow) glUniform1i(pass == TRAINING_IMPOSTOR ? locImpStableBroadleaf : locStableBroadleaf, 0);
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
        if(mesh.lobbyImpostor) glDeleteTextures(1,&mesh.lobbyImpostor);
        mesh=SpruceMesh{};
    }
    if(trainingBranchTex) glDeleteTextures(1,&trainingBranchTex);
    for(auto& texture:importedTreeTex) { if(texture) glDeleteTextures(1,&texture); texture=0; }
    if(trainingBroadleafTex) glDeleteTextures(1,&trainingBroadleafTex);
    trainingBranchTex=trainingBroadleafTex=0;
}
