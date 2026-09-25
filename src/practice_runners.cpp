#include "practice_runners.h"
#include "physics.h"
#include <cstdio>

bool PracticeRunners::start(GameState& game,const glm::vec3& center) {
    GameState staged=game;
    staged.usedMask=1;
    for(int i=1;i<MAX_PLAYERS;++i) {
        glm::vec3 spawn;
        if(!findRunnerSpawn(staged,i,center,spawn)) {
            fprintf(stderr,"[runners] no clear spawn for player %d; move to a more open area\n",i);
            return false;
        }
        staged.players[i]=Player{};
        staged.players[i].pos=spawn;
        staged.players[i].yaw=float(i)*360.0f/MAX_PLAYERS;
        staged.usedMask|=uint16_t(1u<<i);
    }
    home=center;
    for(int i=1;i<MAX_PLAYERS;++i) {
        game.players[i]=staged.players[i];
        runners[i].reset(0x71a91u+uint32_t(i)*7919u,home);
    }
    game.usedMask=staged.usedMask; active=true;
    printf("[runners] 15 runners + you = 16 players; offline, movement only\n");
    return true;
}

void PracticeRunners::stop(GameState& game,const glm::vec3& dummyPosition) {
    active=false; game.usedMask=3;
    for(int i=1;i<MAX_PLAYERS;++i) game.players[i]=Player{};
    game.players[1].pos=dummyPosition;
    printf("[runners] off; practice dummy restored\n");
}

void PracticeRunners::tick(GameState& game,float dt) {
    if(!active) return;
    for(int i=1;i<MAX_PLAYERS;++i) {
        Player& p=game.players[i];
        if(!p.alive) {
            p.respawnTimer-=dt;
            if(p.respawnTimer>0) continue;
            glm::vec3 spawn;
            if(!findRunnerSpawn(game,i,home,spawn)) { p.respawnTimer=1; continue; }
            int kills=p.kills,deaths=p.deaths;
            p=Player{}; p.pos=spawn; p.kills=kills; p.deaths=deaths;
            runners[i].reset(0x71a91u+uint32_t(i)*7919u+uint32_t(deaths),home);
        }
        movePlayer(p,runners[i].input(p,dt),dt);
    }
}
