#include "random_runner.h"
#include "physics.h"
#include "map.h"
#include <cmath>

static bool dryGround(float x,float z) {
    if(fabsf(x)>=gArenaHalf-2 || fabsf(z)>=gArenaHalf-2) return false;
    if(gMapId==MAP_LOBBY && trainingPondRadius(x,z)>1.8f) return true;
    float water=gMapId==MAP_LOBBY ? TRAINING_POND_Y : SEA_LEVEL;
    return terrainHeight(x,z)>=water+.15f;
}

static bool clearGround(glm::vec3& point) {
    if(!dryGround(point.x,point.z)) return false;
    point.y=terrainHeight(point.x,point.z);
    float dx=(terrainHeight(point.x+1,point.z)-terrainHeight(point.x-1,point.z))*.5f;
    float dz=(terrainHeight(point.x,point.z+1)-terrainHeight(point.x,point.z-1))*.5f;
    if(dx*dx+dz*dz>.49f) return false;
    Player probe; probe.pos=point;
    InputState idle{};
    movePlayer(probe,idle,0);
    return glm::distance(probe.pos,point)<.01f;
}

float RandomRunner::random() {
    randomState^=randomState<<13; randomState^=randomState>>17; randomState^=randomState<<5;
    return float(randomState&0xffffff)/16777216.0f;
}

void RandomRunner::reset(uint32_t seed,const glm::vec3& origin) {
    *this=RandomRunner{};
    randomState=seed ? seed : 1;
    home=target=lastPos=origin;
}

void RandomRunner::chooseTarget() {
    standing=random()<.30f;
    decisionTimer=standing ? 2+5*random() : 4+6*random();
    stuckTimer=0;
    if(standing) { sprint=false; return; }
    target=home;
    for(int attempt=0;attempt<12;++attempt) {
        float angle=glm::radians(random()*360.0f),radius=8+32*sqrtf(random());
        glm::vec3 candidate=home+glm::vec3(cosf(angle)*radius,0,sinf(angle)*radius);
        if(clearGround(candidate)) { target=candidate; break; }
    }
    sprint=random()<.4f;
}

InputState RandomRunner::input(const Player& p,float dt) {
    InputState in{};
    in.yaw=p.yaw; in.weaponId=p.weaponId;
    if(!p.alive || dt<=0) return in;
    decisionTimer-=dt;
    if(standing) {
        lastPos=p.pos; stuckTimer=0;
        if(decisionTimer>0) return in;
        chooseTarget();
    } else {
        glm::vec2 travelled(p.pos.x-lastPos.x,p.pos.z-lastPos.z);
        stuckTimer=glm::length(travelled)<dt ? stuckTimer+dt : 0;
        lastPos=p.pos;
        glm::vec2 delta(target.x-p.pos.x,target.z-p.pos.z);
        if(decisionTimer<=0 || stuckTimer>.8f || glm::length(delta)<2) chooseTarget();
    }
    if(standing) return in;
    glm::vec2 delta;
    if(glm::length(glm::vec2(p.pos.x-home.x,p.pos.z-home.z))>48) target=home;
    delta={target.x-p.pos.x,target.z-p.pos.z};
    float wanted=glm::degrees(atan2f(delta.y,delta.x));
    float turn=fmodf(wanted-p.yaw+540.0f,360.0f)-180.0f;
    in.yaw=fmodf(p.yaw+glm::clamp(turn,-150*dt,150*dt)+360.0f,360.0f);
    in.w=fabsf(turn)<65; in.sprint=sprint;
    // Stop and choose another direction before walking into deep water.
    float a=glm::radians(in.yaw);
    if(!dryGround(p.pos.x+cosf(a)*1.5f,p.pos.z+sinf(a)*1.5f)) {
        in.w=false; decisionTimer=0;
    }
    return in;
}

bool findRunnerSpawn(const GameState& game,int slot,const glm::vec3& home,glm::vec3& result) {
    // Stratify the first attempts so the population covers the landscape instead
    // of orbiting the human player. Fall back map-wide for flooded/blocked sectors.
    int sector=slot-1;
    if(sector>=10) ++sector; // Leave a central sector for the human, cover every corner.
    int hx=int(floorf(home.x)),hz=int(floorf(home.z));
    float span=gArenaHalf-8;
    for(int attempt=0;attempt<192;++attempt) {
        float u=mapRand(slot*193+attempt,hx,701);
        float v=mapRand(slot*193+attempt,hz,702);
        if(attempt<64) {
            u=((sector%4)+.10f+.80f*u)/4;
            v=((sector/4)+.10f+.80f*v)/4;
        }
        glm::vec3 point{(u*2-1)*span,0,(v*2-1)*span};
        if(!clearGround(point)) continue;
        bool occupied=false;
        for(int i=0;i<MAX_PLAYERS;++i) {
            if(i==slot || !(game.usedMask&(1u<<i)) || !game.players[i].alive) continue;
            glm::vec2 d(point.x-game.players[i].pos.x,point.z-game.players[i].pos.z);
            if(glm::dot(d,d)<4) { occupied=true; break; }
        }
        if(!occupied) { result=point; return true; }
    }
    return false;
}
