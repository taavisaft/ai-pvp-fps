#include "vegetation.h"
#include "map.h"
#include "meadow_density.h"
#include <algorithm>
#include <array>

// Refill only preallocated buffers. No container growth or GL object creation
// occurs while walking. Tile-coordinate seeds survive eviction and revisits.
void Vegetation::buildWorldGrassTile(int slot, int tx, int tz) {
    GrassTile& tile=meadowTiles[slot];
    std::array<std::array<float,8>,900> plants;
    int boxes[MAX_MAP_BOXES], boxCount=0, count=0;
    const float x0=tx*5.0f, z0=tz*5.0f;
    for(int j=0;j<gMapBoxCount;++j) {
        const Box& b=gMapBoxes[j];
        if(fabsf(x0+2.5f-b.center.x)<b.half.x+2.8f &&
           fabsf(z0+2.5f-b.center.z)<b.half.z+2.8f) boxes[boxCount++]=j;
    }
    tile.minY=1e9f; tile.maxY=-1e9f;
    for(int i=0;i<(meadowCards ? 450 : 900);++i) {
        int key=tx*1024+i;
        float x=x0+mapRand(key,tz,211)*5, z=z0+mapRand(key,tz,212)*5;
        if(fabsf(x)>gArenaHalf || fabsf(z)>gArenaHalf) continue;
        float h=terrainHeight(x,z);
        if(h<1.25f || h>115.0f) continue;
        bool blocked=false;
        for(int k=0;k<boxCount;++k) {
            const Box& b=gMapBoxes[boxes[k]];
            if(fabsf(x-b.center.x)<b.half.x+.3f && fabsf(z-b.center.z)<b.half.z+.3f) {
                blocked=true; break;
            }
        }
        if(blocked) continue;
        glm::vec3 n=glm::normalize(glm::vec3(terrainHeight(x-.25f,z)-terrainHeight(x+.25f,z),
                                            .5f,terrainHeight(x,z-.25f)-terrainHeight(x,z+.25f)));
        // Same slope, dirt and pine masks used by the Paldiski terrain material.
        float slope=(1-n.y)*6+(vegFbm(x*.06f,z*.06f)-.5f)*.10f;
        float rock=lobbySmooth(.30f,.55f,slope);
        float dirt=lobbySmooth(.52f,.70f,vegFbm(x*.004f,z*.004f))*.35f;
        float keep=(1-rock)*(1-dirt)*(1-.8f*pineForestBiome(x,z));
        if(mapRand(key,tz,213)>=keep) continue;
        float dry=.12f+.5f*vegFbm(x*.19f,z*.19f);
        if(meadowCards) {
            int nx=(int)((n.x*.5f+.5f)*255+.5f), nz=(int)((n.z*.5f+.5f)*255+.5f);
            dry=(float)(nx+256*nz);
        }
        plants[count++]={x,h-.025f,z,.65f+.5f*mapRand(key,tz,214),
                         mapRand(key,tz,215)*6.2831853f,mapRand(key,tz,216),
                         .85f+.25f*mapRand(key,tz,217),dry};
        tile.minY=fminf(tile.minY,h); tile.maxY=fmaxf(tile.maxY,h);
    }
    std::sort(plants.begin(),plants.begin()+count,[](const auto& a,const auto& b) {
        return meadowRank(a[5])<meadowRank(b[5]);
    });
    for(int i=0;i<count;++i) meadowRanks[slot][i]=meadowRank(plants[i][5]);
    glBindBuffer(GL_ARRAY_BUFFER,tile.vbo);
    if(count) glBufferSubData(GL_ARRAY_BUFFER,0,count*8*sizeof(float),plants.data());
    tile.count=count; tile.tx=tx; tile.tz=tz;
}

void Vegetation::updateWorldGrass(const glm::vec3& eye) {
    int cx=(int)floorf(eye.x/5), cz=(int)floorf(eye.z/5), budget=6;
    // Fill nearest rings first after spawn/teleport. At normal walking speeds
    // the outer preload ring is ready before its blades become visible.
    for(int ring=0;ring<=11 && budget>0;++ring)
        for(int dz=-ring;dz<=ring && budget>0;++dz)
            for(int dx=-ring;dx<=ring && budget>0;++dx) {
                if(std::max(abs(dx),abs(dz))!=ring) continue;
                int tx=cx+dx, tz=cz+dz;
                int slot=meadowTileSlot(tx,tz);
                GrassTile& t=meadowTiles[slot];
                if(t.tx==tx && t.tz==tz) continue;
                buildWorldGrassTile(slot,tx,tz); --budget;
            }
}
