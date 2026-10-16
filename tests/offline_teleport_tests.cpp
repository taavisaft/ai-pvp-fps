#include "offline_teleport.h"
#include "physics.h"
#include "map.h"
#include "tree_scatter.h"
#include <cstdio>

static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)

int main() {
    setMap(MAP_LOBBY);
    GameState game{}; game.usedMask=1;
    Player& p=game.players[0];
    p.pos={0,terrainHeight(0,136),136}; p.mag=7; p.hp=63;
    uint32_t seed=12345;
    const glm::vec3 start=p.pos;
    CHECK(!teleportOfflinePlayer(game,true,seed));
    CHECK(p.pos==start && seed==12345);
    int quadrants[4]{};
    for(int i=0;i<128;++i) {
        glm::vec3 before=p.pos;
        p.velY=-15; p.airVX=5; p.airVZ=-4;
        CHECK(teleportOfflinePlayer(game,false,seed));
        CHECK(glm::length(glm::vec2(p.pos.x-before.x,p.pos.z-before.z))>=128);
        CHECK(fabsf(p.pos.x)<gArenaHalf-7 && fabsf(p.pos.z)<gArenaHalf-7);
        CHECK(p.pos.y==terrainHeight(p.pos.x,p.pos.z));
        CHECK(trainingPondRadius(p.pos.x,p.pos.z)>=1.8f || p.pos.y>=TRAINING_POND_Y+.3f);
        CHECK(p.velY==0 && p.airVX==0 && p.airVZ==0);
        CHECK(p.mag==7 && p.hp==63);
        // The real collision resolver must leave each landing point unchanged.
        Player probe=p; InputState idle{};
        movePlayer(probe,idle,0);
        CHECK(glm::distance(probe.pos,p.pos)<.001f);
        ++quadrants[(p.pos.x>0 ? 1 : 0)+(p.pos.z>0 ? 2 : 0)];
    }
    for(int count:quadrants) CHECK(count>5);

    // A blocked world must terminate without moving or resetting the player.
    Box blocked{{0,0,0},{gArenaHalf,1000,gArenaHalf}};
    gMapBoxes=&blocked; gMapBoxCount=1;
    const auto before=p;
    CHECK(!teleportOfflinePlayer(game,false,seed));
    CHECK(p.pos==before.pos && p.hp==before.hp && p.mag==before.mag);
    p.alive=false;
    CHECK(!teleportOfflinePlayer(game,false,seed));
    return failures ? 1 : 0;
}
