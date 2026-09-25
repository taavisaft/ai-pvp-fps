#pragma once
#include "random_runner.h"

class PracticeRunners {
public:
    bool enabled() const { return active; }
    bool start(GameState& game,const glm::vec3& center);
    void stop(GameState& game,const glm::vec3& dummyPosition);
    void tick(GameState& game,float dt);
private:
    bool active=false;
    glm::vec3 home{0};
    RandomRunner runners[MAX_PLAYERS];
};
