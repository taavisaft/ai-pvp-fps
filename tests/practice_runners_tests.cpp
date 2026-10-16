#include "practice_runners.h"
#include "map.h"
#include "physics.h"
#include <chrono>
#include <cstdio>

static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)

int main() {
    static GameState game,repeat;
    for(MapId map:{MAP_LOBBY,MAP_PALDISKI}) {
        setMap(map);
        game=GameState{}; game.usedMask=3;
        // Exercise free roaming away from the intentionally enclosed catalogue.
        glm::vec3 home=map==MAP_LOBBY ? glm::vec3(-70,0,80) : gMapSpawns[0];
        home.y=terrainHeight(home.x,home.z);
        game.players[0].pos=home;
        PracticeRunners crowd,again;
        CHECK(crowd.start(game,home)); CHECK(game.usedMask==0xffff);
        CHECK(game.players[0].pos==home);
        for(int i=1;i<MAX_PLAYERS;++i) for(int j=0;j<i;++j)
            CHECK(glm::length(glm::vec2(game.players[i].pos.x-game.players[j].pos.x,
                                        game.players[i].pos.z-game.players[j].pos.z))>=2);
        int quadrants[4]{};
        float minX=gArenaHalf,maxX=-gArenaHalf,minZ=gArenaHalf,maxZ=-gArenaHalf;
        for(int i=1;i<MAX_PLAYERS;++i) {
            const auto& p=game.players[i];
            ++quadrants[(p.pos.x>0 ? 1 : 0)+(p.pos.z>0 ? 2 : 0)];
            minX=fminf(minX,p.pos.x); maxX=fmaxf(maxX,p.pos.x);
            minZ=fminf(minZ,p.pos.z); maxZ=fmaxf(maxZ,p.pos.z);
            Player probe=p; InputState idle{}; movePlayer(probe,idle,0);
            CHECK(glm::distance(probe.pos,p.pos)<.01f);
            CHECK(p.pos.y==terrainHeight(p.pos.x,p.pos.z));
            if(map==MAP_LOBBY)
                CHECK(trainingPondRadius(p.pos.x,p.pos.z)>=1.8f || p.pos.y>=TRAINING_POND_Y+.15f);
            else CHECK(p.pos.y>=SEA_LEVEL+.15f);
        }
        for(int count:quadrants) CHECK(count>0);
        CHECK(maxX-minX>gArenaHalf && maxZ-minZ>gArenaHalf);
        repeat=game; CHECK(again.start(repeat,home));
        float travelled[MAX_PLAYERS]{};
        auto start=std::chrono::steady_clock::now();
        for(int tick=0;tick<1800;++tick) {
            glm::vec3 before[MAX_PLAYERS];
            for(int i=1;i<MAX_PLAYERS;++i) before[i]=game.players[i].pos;
            crowd.tick(game,1.0f/60); again.tick(repeat,1.0f/60);
            for(int i=1;i<MAX_PLAYERS;++i) {
                const auto& p=game.players[i];
                CHECK(std::isfinite(p.pos.x+p.pos.y+p.pos.z));
                CHECK(p.pos==repeat.players[i].pos);
                CHECK(fabsf(p.pos.x)<gArenaHalf && fabsf(p.pos.z)<gArenaHalf);
                travelled[i]+=glm::length(p.pos-before[i]);
            }
        }
        double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        printf("[runners test] map=%d two 15-runner simulations+checks: %.3f ms/tick\n",int(map),ms/1800);
        for(int i=1;i<MAX_PLAYERS;++i) CHECK(travelled[i]>20);
        CHECK(game.players[0].pos==home); CHECK(game.bulletCount==0);
        // Verify intentional pauses as well as both moving speeds, rather than
        // counting turning/obstacle stalls as standing behavior.
        RandomRunner driver; Player probe=game.players[1];
        driver.reset(7919,probe.pos);
        int standing=0,walking=0,running=0,stillRun=0,longestStill=0;
        for(int tick=0;tick<7200;++tick) {
            InputState in=driver.input(probe,1.0f/60);
            CHECK(!in.shoot && !in.shootHeld);
            bool still=!in.w && !in.sprint && in.yaw==probe.yaw;
            if(still) { ++standing; ++stillRun; } else stillRun=0;
            if(stillRun>longestStill) longestStill=stillRun;
            if(in.w) { if(in.sprint) ++running; else ++walking; }
            movePlayer(probe,in,1.0f/60);
        }
        CHECK(standing>60 && walking>60 && running>60 && longestStill>=120);

        game.players[5].alive=false; game.players[5].hp=0;
        game.players[5].respawnTimer=RESPAWN_TIME; game.players[5].deaths=3;
        for(int i=0;i<120;++i) crowd.tick(game,1.0f/60);
        CHECK(!game.players[5].alive);
        for(int i=0;i<62;++i) crowd.tick(game,1.0f/60);
        CHECK(game.players[5].alive && game.players[5].hp==PLAYER_HP);
        CHECK(game.players[5].deaths==3);
        auto before=game.players[1].pos;
        CHECK(!crowd.start(game,{10000,0,10000}));
        CHECK(game.players[1].pos==before && game.usedMask==0xffff);
        crowd.stop(game,home+glm::vec3(4,0,0));
        CHECK(!crowd.enabled() && game.usedMask==3);
        CHECK(game.players[1].pos==home+glm::vec3(4,0,0));
    }
    return failures ? 1 : 0;
}
