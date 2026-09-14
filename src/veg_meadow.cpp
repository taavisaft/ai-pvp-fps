#include "vegetation.h"
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
    if(gMapId!=MAP_LOBBY) {
        meadowEnabled=getenv("FPS_NOMEADOW")==nullptr;
        meadowSide=24;
    }
    meadowRanks.resize(meadowSide*meadowSide);
    bufTile.reserve(5*5*36*8);
    if(gMapId!=MAP_LOBBY) {
        for(int i=0;i<24*24;++i) {
            GrassTile& t=meadowTiles[i];
            t.tx=t.tz=INT_MIN; t.count=0;
            if(!t.vbo) glGenBuffers(1,&t.vbo);
            glBindBuffer(GL_ARRAY_BUFFER,t.vbo);
            glBufferData(GL_ARRAY_BUFFER,900*8*sizeof(float),nullptr,GL_DYNAMIC_DRAW);
            if(!t.vao) t.vao=vegMakeVAO(meadowVbo,meadowEbo,t.vbo);
            if(!meadowFarVao[i]) meadowFarVao[i]=vegMakeVAO(meadowFarVbo,meadowFarEbo,t.vbo);
        }
        printf("[meadow] Paldiski streaming: 576 reserved tiles, 50 m draw / 55 m preload\n");
        return;
    }
    int total=0;
    for (int iz=0;iz<meadowSide;++iz) for(int ix=0;ix<meadowSide;++ix) {
        GrassTile& t=meadowTiles[iz*meadowSide+ix];
        bufTile.clear(); t.minY=1e9f; t.maxY=-1e9f;
        for (int i=0;i<(meadowCards ? 450 : 900);++i) {
            int key=(iz*meadowSide+ix)*1024+i;
            float x=(meadowFull ? -60 : -10)+ix*5+mapRand(key,0,211)*5;
            float z=(meadowFull ? -60 : 20)+iz*5+mapRand(key,0,212)*5;
            float growth=lobbyGrowth(x,z);
            float keep=lobbyGrowthDensity(growth)*(meadowFull ? 1.0f : lobbyMeadow(x,z))*(1-lobbyWear(x,z));
            if (mapRand(key,0,213)>keep) continue;
            float h=terrainHeight(x,z);
            bool blocked=false;
            for (int j=0;j<gMapBoxCount;++j) {
                const Box& b=gMapBoxes[j];
                if(fabsf(x-b.center.x)<b.half.x+.3f && fabsf(z-b.center.z)<b.half.z+.3f) blocked=true;
            }
            if(blocked) continue;
            float scale=(.25f+.90f*growth)*(.8f+.4f*mapRand(key,0,214));
            float dry=.12f+.5f*vegFbm(x*.19f,z*.19f);
            if(meadowCards) {
                glm::vec3 n=glm::normalize(glm::vec3(terrainHeight(x-.25f,z)-terrainHeight(x+.25f,z),
                                                    .5f,terrainHeight(x,z-.25f)-terrainHeight(x,z+.25f)));
                int nx=(int)((n.x*.5f+.5f)*255+.5f), nz=(int)((n.z*.5f+.5f)*255+.5f);
                dry=(float)(nx+256*nz); // exact packed terrain normal in instance B.w
            }
            bufTile.insert(bufTile.end(),{x,h-.025f,z,scale,mapRand(key,0,215)*6.2831853f,
                                         mapRand(key,0,216),.85f+.25f*mapRand(key,0,217),dry});
            t.minY=fminf(t.minY,h); t.maxY=fmaxf(t.maxY,h);
        }
        // Fixed random ordering lets a distant draw submit only a prefix.
        // Build once; camera movement never uploads or reorders instance buffers.
        std::vector<std::array<float,8>> ordered(bufTile.size()/8);
        for(size_t i=0;i<ordered.size();++i)
            std::copy_n(bufTile.data()+i*8,8,ordered[i].begin());
        std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b) {
            return meadowRank(a[5]) < meadowRank(b[5]);
        });
        for(size_t i=0;i<ordered.size();++i) {
            std::copy_n(ordered[i].begin(),8,bufTile.data()+i*8);
            meadowRanks[iz*meadowSide+ix][i]=meadowRank(ordered[i][5]);
        }
        if(!t.vbo) glGenBuffers(1,&t.vbo);
        glBindBuffer(GL_ARRAY_BUFFER,t.vbo);
        glBufferData(GL_ARRAY_BUFFER,bufTile.size()*sizeof(float),bufTile.data(),GL_STATIC_DRAW);
        if(!t.vao) t.vao=vegMakeVAO(meadowVbo,meadowEbo,t.vbo);
        GLuint& farVao=meadowFarVao[iz*meadowSide+ix];
        if(!farVao) farVao=vegMakeVAO(meadowFarVbo,meadowFarEbo,t.vbo);
        t.count=(int)bufTile.size()/8; total+=t.count;
    }
    printf("[meadow] %d mixed plants, %dx%d m, %d static tiles\n",
           total,meadowSide*5,meadowSide*5,meadowSide*meadowSide);
    printf("[meadow] %s, near %d / far %d triangles per clump\n",
           meadowCards ? "tapered meadow blades" : "ribbons",(meadowCards ? trainingIdx : meadowIdx)/3,(meadowCards ? trainingFarIdx : meadowFarIdx)/3);
}

void Vegetation::drawMeadow(const Frustum& fr, const glm::vec3& eye, bool shadow) {
    if(!meadowEnabled || shadow) return;
    const bool world=gMapId!=MAP_LOBBY;
    if(world) updateWorldGrass(eye);
    vegSh.setFloat(locWind,.045f);
    vegSh.setFloat(locRange,world ? 60.0f : 38.0f);
    glUniform2f(locFadeIn,0,0); glUniform2f(locFadeOut,0,0);
    if(meadowCards) {
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D,meadowAtlas);
        glActiveTexture(GL_TEXTURE0);
    }
    if(shadow) {
        vegDepthSh.setFloat(locWindD,.045f);
        vegDepthSh.setVec3(locMeadowEye,eye);
        vegDepthSh.setFloat(locMeadowRange,38);
    }
    MeadowTimer& timer=meadowTimer[shadow ? 1 : 0];
    timer.begin();
    for(int iz=0;iz<meadowSide;++iz) for(int ix=0;ix<meadowSide;++ix) {
        const GrassTile& t=meadowTiles[iz*meadowSide+ix];
        if(!t.count || (world && t.tx==INT_MIN)) continue;
        glm::vec3 center((meadowFull ? -57.5f : -7.5f)+ix*5,
                         (t.minY+t.maxY)*.5f,(meadowFull ? -57.5f : 22.5f)+iz*5);
        if(world) center=glm::vec3(t.tx*5+2.5f,(t.minY+t.maxY)*.5f,t.tz*5+2.5f);
        float dx=fmaxf(fabsf(eye.x-center.x)-2.5f,0), dz=fmaxf(fabsf(eye.z-center.z)-2.5f,0);
        float plantHeight=world ? 1.0f : 1.5f; // include the training seed stalks
        if(!fr.aabbVisible(center,{4.0f,(t.maxY-t.minY)*.5f+plantHeight,4.0f})) continue;
        const float* ranks=meadowRanks[iz*meadowSide+ix].data();
        // Nearest tile point is conservative: per-plant shader thinning handles the
        // rest, identically in visible and shadow passes.
        float distance=sqrtf(dx*dx+dz*dz);
        float density=world ? worldMeadowDensity(distance) : meadowDensity(distance);
        int count=(int)(std::lower_bound(ranks,ranks+t.count,density)-ranks);
        if(!count) continue;
        if(dx*dx+dz*dz < 32*32) {
            glBindVertexArray(t.vao);
            const bool training=!world && meadowCards;
            glDrawElementsInstanced(GL_TRIANGLES,training ? trainingIdx : meadowIdx,GL_UNSIGNED_INT,
                training ? reinterpret_cast<const void*>(size_t(meadowIdx)*sizeof(unsigned)) : nullptr,count);
        }
        float farX=fabsf(eye.x-center.x)+2.5f, farZ=fabsf(eye.z-center.z)+2.5f;
        if(farX*farX+farZ*farZ > 18*18) {
            glBindVertexArray(meadowFarVao[iz*meadowSide+ix]);
            const bool training=!world && meadowCards;
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
