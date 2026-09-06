#include "vegetation.h"
#include "map.h"
#include <cstdio>
#include <array>
#include <algorithm>
#include "meadow_density.h"

void Vegetation::prepareMeadow() {
    if (getenv("FPS_MEADOW_GPU")) for(auto& timer:meadowTimer) timer.init();
    if (!meadowVbo) {
        std::vector<float> verts;
        std::vector<unsigned> indices;
        vegBuildMeadow(verts,indices);
        glGenBuffers(1,&meadowVbo); glGenBuffers(1,&meadowEbo);
        glBindBuffer(GL_ARRAY_BUFFER,meadowVbo);
        glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,meadowEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned),indices.data(),GL_STATIC_DRAW);
        meadowIdx=(GLsizei)indices.size();
    }
    bufTile.reserve(5*5*36*8);
    int total=0;
    for (int iz=0;iz<4;++iz) for(int ix=0;ix<4;++ix) {
        GrassTile& t=meadowTiles[iz*4+ix];
        bufTile.clear(); t.minY=1e9f; t.maxY=-1e9f;
        for (int i=0;i<900;++i) {
            int key=(iz*4+ix)*1024+i;
            float x=-10+ix*5+mapRand(key,0,211)*5;
            float z=20+iz*5+mapRand(key,0,212)*5;
            float keep=lobbyMeadow(x,z)*(1-lobbyWear(x,z));
            if (mapRand(key,0,213)>keep) continue;
            float h=terrainHeight(x,z);
            bool blocked=false;
            for (int j=0;j<gMapBoxCount;++j) {
                const Box& b=gMapBoxes[j];
                if(fabsf(x-b.center.x)<b.half.x+.3f && fabsf(z-b.center.z)<b.half.z+.3f) blocked=true;
            }
            if(blocked) continue;
            float scale=.65f+.5f*mapRand(key,0,214);
            float dry=.12f+.5f*vegFbm(x*.19f,z*.19f);
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
            meadowRanks[iz*4+ix][i]=meadowRank(ordered[i][5]);
        }
        if(!t.vbo) glGenBuffers(1,&t.vbo);
        glBindBuffer(GL_ARRAY_BUFFER,t.vbo);
        glBufferData(GL_ARRAY_BUFFER,bufTile.size()*sizeof(float),bufTile.data(),GL_STATIC_DRAW);
        if(!t.vao) t.vao=vegMakeVAO(meadowVbo,meadowEbo,t.vbo);
        t.count=(int)bufTile.size()/8; total+=t.count;
    }
    printf("[meadow] %d mixed plants, 20x20 m, 16 static tiles\n",total);
}

void Vegetation::drawMeadow(const Frustum& fr, const glm::vec3& eye, bool shadow) {
    if(gMapId!=MAP_LOBBY || !meadowEnabled) return;
    if(shadow) {
        vegDepthSh.setFloat(locWindD,.045f);
        vegDepthSh.setVec3(locMeadowEye,eye);
        vegDepthSh.setFloat(locMeadowRange,38);
    }
    MeadowTimer& timer=meadowTimer[shadow ? 1 : 0];
    timer.begin();
    for(int iz=0;iz<4;++iz) for(int ix=0;ix<4;++ix) {
        const GrassTile& t=meadowTiles[iz*4+ix];
        if(!t.count) continue;
        glm::vec3 center(-7.5f+ix*5,(t.minY+t.maxY)*.5f,22.5f+iz*5);
        float dx=fmaxf(fabsf(eye.x-center.x)-2.5f,0), dz=fmaxf(fabsf(eye.z-center.z)-2.5f,0);
        if(dx*dx+dz*dz>38*38) continue;
        if(!fr.aabbVisible(center,{2.9f,(t.maxY-t.minY)*.5f+1,2.9f})) continue;
        const float* ranks=meadowRanks[iz*4+ix];
        // Nearest tile point is conservative: per-plant shader fade handles the
        // rest, identically in visible and shadow passes.
        int count=(int)(std::lower_bound(ranks,ranks+t.count,meadowDensity(sqrtf(dx*dx+dz*dz)))-ranks);
        if(!count) continue;
        glBindVertexArray(t.vao);
        glDrawElementsInstanced(GL_TRIANGLES,meadowIdx,GL_UNSIGNED_INT,nullptr,count);
    }
    glBindVertexArray(0);
    timer.end();
}

void Vegetation::destroyMeadow() {
    meadowTimer[0].destroy("lit"); meadowTimer[1].destroy("shadow");
    for(auto& t:meadowTiles) {
        if(t.vao) glDeleteVertexArrays(1,&t.vao);
        if(t.vbo) glDeleteBuffers(1,&t.vbo);
        t=GrassTile{};
    }
    if(meadowVbo) glDeleteBuffers(1,&meadowVbo);
    if(meadowEbo) glDeleteBuffers(1,&meadowEbo);
    meadowVbo=meadowEbo=0;
}
