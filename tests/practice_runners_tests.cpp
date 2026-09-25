#include "practice_runners.h"
#include "map.h"
#include <chrono>
#include <cstdio>

static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)

int main() {
    static GameState game,repeat;
    for(MapId map:{MAP_LOBBY,MAP_PALDISKI}) {
        setMap(map);
        game=GameState{}; game.usedMask=3;
        glm::vec3 home=gMapSpawns[0]; home.y=terrainHeight(home.x,home.z);
        game.players[0].pos=home;
        PracticeRunners crowd,again;
        CHECK(crowd.start(game,home)); CHECK(game.usedMask==0xffff);
        CHECK(game.players[0].pos==home);
        for(int i=1;i<MAX_PLAYERS;++i) for(int j=0;j<i;++j)
            CHECK(glm::length(glm::vec2(game.players[i].pos.x-game.players[j].pos.x,
                                        game.players[i].pos.z-game.players[j].pos.z))>=2);
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
