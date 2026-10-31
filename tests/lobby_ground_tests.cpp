#include "map.h"
#include "physics.h"
#include "tree_scatter.h"
#include "meadow_density.h"
#include "lobby_growth.h"
#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while (0)
int main() {
    // Baseline samples from commit 9505aca: online heightfield stays unchanged.
    const glm::vec2 samples[] = {{0,0},{325,75},{-301,527},{120,-240}};
    const float baseline[] = {.287819326f,4.1805234f,40.7698097f,-3.78697681f};
    for (int i=0; i<4; ++i) CHECK(fabsf(paldiskiElevation(samples[i].x,samples[i].y)-baseline[i]) < .0001f);
    CHECK(meadowDensity(0) > 1 && meadowDensity(6) > 1);
    CHECK(meadowDensity(28) > .13f && meadowDensity(28) < .14f);
    CHECK(meadowDensity(60) > .025f && meadowDensity(60) < .026f);
    CHECK(meadowDensity(170) == .004f); // far lobby still has grass
    for(int d=0;d<180;++d) CHECK(meadowDensity(d) >= meadowDensity(d+1));
    for(int i=0;i<100;++i) CHECK(meadowRank(i*.01f)>=0 && meadowRank(i*.01f)<1);
    CHECK(worldMeadowDensity(6)==meadowDensity(6));
    CHECK(worldMeadowDensity(45)>0 && worldMeadowDensity(45)<meadowDensity(45));
    CHECK(worldMeadowDensity(50)==0 && worldMeadowDensity(100)==0);
    for(int d=0;d<60;++d) CHECK(worldMeadowDensity(d)>=worldMeadowDensity(d+1));
    const int centers[]={-205,-1,0,1,205};
    for(int center : centers) {
        bool slots[576]{};
        for(int z=center-11;z<=center+11;++z) for(int x=center-11;x<=center+11;++x) {
            int slot=meadowTileSlot(x,z);
            CHECK(slot>=0 && slot<576);
            CHECK(!slots[slot]); slots[slot]=true;
        }
    }
    int sparse=0, dense=0;
    for(int z=-60;z<=60;z+=2) for(int x=-60;x<=60;x+=2) {
        float g=lobbyGrowth((float)x,(float)z);
        CHECK(g>=0 && g<=1);
        sparse+=g<.15f; dense+=g>.8f;
    }
    CHECK(sparse>100 && dense>100);
    CHECK(lobbyGrowthDensity(0)<.02f && lobbyGrowthDensity(1)==1);
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
    glm::vec3 spawn = gMapSpawns[0];
    CHECK(spawn.x*spawn.x+spawn.z*spawn.z > 120*120);
    float spawnEye = terrainHeight(spawn.x,spawn.z)+1.7f;
    CHECK(terrainHeight(0,195) < TRAINING_POND_Y);
    // Broad walkable hills retain low fields and the pond basin.
    float fieldMin=100, fieldMax=-100, fieldGrade=0;
    for(int z=-1024;z<=1024;z+=16) for(int x=-1024;x<=1024;x+=16) {
        if(x*x+z*z<220*220) continue;
        float h=terrainHeight((float)x,(float)z);
        CHECK(std::isfinite(h));
        fieldMin=fminf(fieldMin,h); fieldMax=fmaxf(fieldMax,h);
        float gx=terrainHeight(x+1.0f,(float)z)-h;
        float gz=terrainHeight((float)x,z+1.0f)-h;
        fieldGrade=fmaxf(fieldGrade,sqrtf(gx*gx+gz*gz));
    }
    CHECK(fieldMax-fieldMin>150 && fieldMax-fieldMin<180);
    CHECK(fieldGrade<.45f);
    std::printf("training relief %.2f m, max sampled grade %.3f\n",fieldMax-fieldMin,fieldGrade);
    CHECK(terrainHeight(TRAINING_OVERLOOK_X,TRAINING_OVERLOOK_Z)-terrainHeight(0,300)>90);
    CHECK(terrainHeight(-780,-220)>140);
    CHECK(terrainHeight(-780,580)>140);
    CHECK(terrainHeight(850,-450)>125);
    CHECK(terrainHeight(0,-450)<20);
    // Ground-level view across 1.46 km of valley remains unobstructed by terrain.
    float overlookEye=terrainHeight(TRAINING_OVERLOOK_X,TRAINING_OVERLOOK_Z)+1.7f;
    float farEye=terrainHeight(700,-100)+1.7f;
    for(int i=1;i<100;++i) {
        float t=i*.01f;
        float x=TRAINING_OVERLOOK_X+(700-TRAINING_OVERLOOK_X)*t;
        float z=TRAINING_OVERLOOK_Z+(-100-TRAINING_OVERLOOK_Z)*t;
        CHECK(overlookEye+(farEye-overlookEye)*t>terrainHeight(x,z)+1.0f);
    }
    int openFields=0, woodland=0, clearBlocks=0, fieldSamples=0;
    for(int z=-900;z<=900;z+=50) for(int x=-900;x<=900;x+=50) {
        float forest=trainingForest((float)x,(float)z);
        CHECK(forest>=0 && forest<=1);
        openFields+=forest<.03f; woodland+=forest>.75f; ++fieldSamples;
        if(forest<.03f && trainingForest(x+100.0f,(float)z)<.03f &&
           trainingForest((float)x,z+100.0f)<.03f && trainingForest(x+100.0f,z+100.0f)<.03f)
            ++clearBlocks;
    }
    CHECK(openFields*4>fieldSamples && openFields*5<fieldSamples*4);
    CHECK(woodland*5>fieldSamples && woodland*5<fieldSamples*3);
    CHECK(clearBlocks>50);
    for (int z=260; z<850; ++z) {
        float h=terrainHeight(0,(float)z);
        CHECK(std::isfinite(h));
        CHECK(fabsf(terrainHeight(0,(float)z+1)-h) < 1.0f);
    }
    for (int i=1; i<100; ++i) {
        float t = i/100.0f, z = spawn.z+(195-spawn.z)*t;
        float sight = spawnEye+(TRAINING_POND_Y-spawnEye)*t;
        if (terrainHeight(0,z) > TRAINING_POND_Y) CHECK(sight-terrainHeight(0,z) > 1.0f);
    }
    for (int a=0; a<360; a+=3) for (float r=56; r<142; r+=1) {
        float c=cosf(a*.0174533f), s=sinf(a*.0174533f);
        float x0=c*r, z0=s*r, x1=c*(r+1), z1=s*(r+1);
        // New building apron intentionally replaces this part of the old blend.
        // Its height continuity is checked by khrushchevka_tests.
        if(fabsf(x0-KHRUSH_X)<KHRUSH_LENGTH*.5f+43 && fabsf(z0-KHRUSH_Z)<50) continue;
        float range=fabsf(trainingRangeHeight(x1,z1)-trainingRangeHeight(x0,z0));
        float land=fabsf(trainingLandscapeHeight(x1,z1)-trainingLandscapeHeight(x0,z0));
        CHECK(fabsf(terrainHeight(x1,z1)-terrainHeight(x0,z0)) < fmaxf(range,land)+.55f);
    }
    CHECK(gTrees.size() > 30000 && gTrees.size() < 64000);
    std::vector<TreeInstance> first = gTrees;
    buildTreeColliders();
    CHECK(first.size() == gTrees.size());
    for (size_t i=0; i<first.size() && i<gTrees.size(); i+=97)
        CHECK(first[i].x==gTrees[i].x && first[i].z==gTrees[i].z && first[i].scale==gTrees[i].scale);
    for (const TreeInstance& tree : gTrees) {
        CHECK(trainingPondRadius(tree.x,tree.z) > 1.1f);
        float dx=tree.x-spawn.x, dz=tree.z-spawn.z;
        CHECK(dx*dx+dz*dz > 6*6);
    }
    setMap(MAP_PALDISKI);
    CHECK(gTrees.size() > 20000 && gTrees.size() < 64000);
    int mature=0;
    for (const TreeInstance& tree : gTrees) {
        CHECK(tree.y > 1.0f && tree.scale >= 2.0f && tree.scale <= 19.5f);
        mature += tree.scale >= 9.0f;
    }
    CHECK(mature*3 > (int)gTrees.size());
    setMap(MAP_LOBBY);
    Player p{}; p.pos = {5,terrainHeight(5,17),17};
    InputState in{}; in.w = true; in.yaw = 90;
    for (int i=0; i<240; ++i) {
        movePlayer(p,in,1.0f/60);
        CHECK(p.pos.y >= terrainHeight(p.pos.x,p.pos.z)-.001f);
    }
    CHECK(p.pos.z > 30);
    CHECK(gNatureFenceCount>700 && gTownBoxCount<MAX_MAP_BOXES);
    for(float z:{124.0f,136.0f}) {
        Player walker{}; walker.pos={0,terrainHeight(0,z),z};
        InputState east{}; east.w=true; east.yaw=0;
        for(int frame=0;frame<120;++frame) movePlayer(walker,east,1.0f/60);
        CHECK(z==136 ? walker.pos.x>6 : walker.pos.x<4);
        float y=terrainHeight(4,z)+1.08f;
        bool hit=false;
        for(int i=gNatureFenceFirst;i<gNatureFenceFirst+gNatureFenceCount;++i) {
            float fraction;
            const auto& b=gMapBoxes[i];
            hit|=segmentAabb({2,y,z},{6,y,z},b.center,b.half,fraction);
        }
        CHECK(hit==(z==124));
    }
    int part=0;
    struct Verify { int* part; } verify{&part};
    visitNatureFenceColliders([](const NatureFencePart& p,void* context) {
        auto& v=*static_cast<Verify*>(context);
        const auto& b=gMapBoxes[gNatureFenceFirst+(*v.part)++];
        CHECK(glm::length(b.center-p.center)<.0001f);
        CHECK(glm::length(b.half-p.half)<.0001f);
    },&verify,trainingHeight);
    CHECK(part==gNatureFenceCount);
    CHECK(gNatureLogCount>100 && gNatureLogCount<200);
    for(int i=0;i<gNatureLogCount;++i) {
        const auto& log=gNatureLogs[i];
        glm::vec3 axis=glm::normalize(log.axis);
        glm::vec3 side=glm::normalize(glm::cross(axis,glm::vec3(0,1,0)));
        // A continuous rail at both post ends and its midpoint; no stair risers.
        for(float along:{-.9f,0.0f,.9f}) {
            glm::vec3 p=log.center+log.axis*along,n; float t;
            CHECK(segmentNatureLog(log,p-side,p+side,t,n));
            CHECK(t>.45f && t<.5f);
            CHECK(!segmentNatureLog(log,p+glm::vec3(0,.2f,0)-side,
                                         p+glm::vec3(0,.2f,0)+side,t,n));
        }
    }
    return failures ? 1 : 0;
}
