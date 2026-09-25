#pragma once
#include "game.h"

// Small deterministic movement driver. All motion/collision remains in movePlayer.
// Each runner owns its PRNG, so it never changes weapon spread or global randomness.
struct RandomRunner {
    void reset(uint32_t seed,const glm::vec3& home);
    InputState input(const Player& player,float dt);
private:
    uint32_t randomState=1;
    glm::vec3 home{0},target{0},lastPos{0};
    float decisionTimer=0,stuckTimer=0;
    bool sprint=false;
    float random();
    void chooseTarget();
};

// Startup/respawn placement only: bounded search, dry ground and shared collision.
bool findRunnerSpawn(const GameState& game,int slot,const glm::vec3& home,glm::vec3& result);
