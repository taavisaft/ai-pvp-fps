#include "map.h"
#include "physics.h"
#include "meadow_density.h"
#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while (0)
int main() {
    // Baseline samples from commit 9505aca: online heightfield stays unchanged.
    const glm::vec2 samples[] = {{0,0},{325,75},{-301,527},{120,-240}};
    const float baseline[] = {.287819326f,4.1805234f,40.7698097f,-3.78697681f};
    for (int i=0; i<4; ++i) CHECK(fabsf(paldiskiElevation(samples[i].x,samples[i].y)-baseline[i]) < .0001f);
    CHECK(meadowDensity(0) > 1 && meadowDensity(6) > 1);
    CHECK(fabsf(meadowDensity(28)-.22f) < .00001f);
    for(int d=0;d<40;++d) CHECK(meadowDensity(d) >= meadowDensity(d+1));
    for(int i=0;i<100;++i) CHECK(meadowRank(i*.01f)>=0 && meadowRank(i*.01f)<1);
    setMap(MAP_LOBBY);
    CHECK(lobbyMeadow(0,30) == 1);
    CHECK(lobbyMeadow(11,30) == 0 && lobbyMeadow(0,41) == 0);
    CHECK(lobbyMeadow(9,30) > 0 && lobbyMeadow(9,30) < 1);
    CHECK(lobbyWear(12,0) == 1);
    CHECK(lobbyWear(0,19) > .9f);
    CHECK(lobbyWear(0,30) == 0);
    CHECK(terrainHeight(12,0) == 0);
    // New low relief belongs to meadow margins, not the shooting lane.
    float minimum = 100, maximum = -100;
    for (int z=22; z<=32; ++z) for (int x=-10; x<=10; ++x) {
        float h = terrainHeight(x,z);
        CHECK(std::isfinite(h) && h >= 0);
        minimum = fminf(minimum,h); maximum = fmaxf(maximum,h);
        // Half-metre rendering grid must remain close to analytic collision.
        float mid = terrainHeight(x+.25f,z+.25f);
        float mesh = (terrainHeight(x+.5f,z)+terrainHeight(x,z+.5f))*.5f;
        CHECK(fabsf(mid-mesh) < .06f);
    }
    CHECK(maximum-minimum > .3f);
    Player p{}; p.pos = {5,terrainHeight(5,17),17};
    InputState in{}; in.w = true; in.yaw = 90;
    for (int i=0; i<240; ++i) {
        movePlayer(p,in,1.0f/60);
        CHECK(p.pos.y >= terrainHeight(p.pos.x,p.pos.z)-.001f);
    }
    CHECK(p.pos.z > 30);
    return failures ? 1 : 0;
}
