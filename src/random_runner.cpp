#include "random_runner.h"
#include "physics.h"
#include "map.h"
#include <cmath>

static bool dryGround(float x,float z) {
    if(fabsf(x)>=gArenaHalf-2 || fabsf(z)>=gArenaHalf-2) return false;
    if(gMapId==MAP_LOBBY && trainingPondRadius(x,z)>1.15f) return true;
    float water=gMapId==MAP_LOBBY ? TRAINING_POND_Y : SEA_LEVEL;
    return terrainHeight(x,z)>=water+.15f;
}

static bool clearGround(glm::vec3& point) {
    if(!dryGround(point.x,point.z)) return false;
    point.y=terrainHeight(point.x,point.z);
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
    target=home;
    for(int attempt=0;attempt<12;++attempt) {
        float angle=glm::radians(random()*360.0f),radius=8+32*sqrtf(random());
        glm::vec3 candidate=home+glm::vec3(cosf(angle)*radius,0,sinf(angle)*radius);
        if(clearGround(candidate)) { target=candidate; break; }
    }
    sprint=random()<.4f;
    decisionTimer=2+3*random(); stuckTimer=0;
}

InputState RandomRunner::input(const Player& p,float dt) {
    InputState in{};
    in.yaw=p.yaw; in.weaponId=p.weaponId;
    if(!p.alive || dt<=0) return in;
    glm::vec2 travelled(p.pos.x-lastPos.x,p.pos.z-lastPos.z);
    stuckTimer=glm::length(travelled)<dt ? stuckTimer+dt : 0;
    lastPos=p.pos;
    decisionTimer-=dt;
    glm::vec2 delta(target.x-p.pos.x,target.z-p.pos.z);
    if(decisionTimer<=0 || stuckTimer>.8f || glm::length(delta)<2) chooseTarget();
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
    for(int attempt=0;attempt<96;++attempt) {
        float angle=glm::radians(float(slot)*137.508f+attempt*47.0f);
        float radius=8+float((slot*7+attempt*3)%25);
        glm::vec3 point=home+glm::vec3(cosf(angle)*radius,0,sinf(angle)*radius);
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
