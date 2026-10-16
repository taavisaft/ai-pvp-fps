#include "offline_teleport.h"
#include "map.h"
#include "tree_scatter.h"

namespace {
float randomUnit(uint32_t& state) {
    if(!state) state=1;
    state^=state<<13; state^=state>>17; state^=state<<5;
    return float(state&0xffffffu)/16777216.0f;
}

bool safe(const GameState& game, const glm::vec3& p) {
    if(gMapId==MAP_LOBBY) {
        if(trainingPondRadius(p.x,p.z)<1.8f && p.y<TRAINING_POND_Y+.3f) return false;
    } else if(p.y<SEA_LEVEL+.3f) return false;
    float dx=(terrainHeight(p.x+1,p.z)-terrainHeight(p.x-1,p.z))*.5f;
    float dz=(terrainHeight(p.x,p.z+1)-terrainHeight(p.x,p.z-1))*.5f;
    if(dx*dx+dz*dz>.49f) return false;
    for(int i=0;i<gMapBoxCount;++i) {
        const auto& b=gMapBoxes[i];
        if(fabsf(p.x-b.center.x)<b.half.x+.8f &&
           fabsf(p.z-b.center.z)<b.half.z+.8f) return false;
    }
    bool blocked=false;
    gTreeColGrid.forEachNear(p.x,p.z,4,[&](int i) {
        const auto& tree=gTreeCols[i];
        float x=p.x-tree.x,z=p.z-tree.z,r=tree.r+1;
        if(x*x+z*z<r*r) blocked=true;
    });
    for(int i=1;i<MAX_PLAYERS;++i) {
        if(!(game.usedMask&(1u<<i)) || !game.players[i].alive) continue;
        glm::vec2 d(p.x-game.players[i].pos.x,p.z-game.players[i].pos.z);
        if(glm::dot(d,d)<4) blocked=true;
    }
    return !blocked;
}
}

bool teleportOfflinePlayer(GameState& game,bool connected,uint32_t& randomState) {
    if(connected || !game.players[0].alive) return false;
    Player& self=game.players[0];
    float span=gArenaHalf-8;
    for(int attempt=0;attempt<256;++attempt) {
        glm::vec3 p{(randomUnit(randomState)*2-1)*span,0,
                    (randomUnit(randomState)*2-1)*span};
        glm::vec2 d(p.x-self.pos.x,p.z-self.pos.z);
        if(glm::dot(d,d)<128*128) continue;
        p.y=terrainHeight(p.x,p.z);
        if(!safe(game,p)) continue;
        self.pos=p;
        self.velY=self.airVX=self.airVZ=0;
        return true;
    }
    return false;
}
