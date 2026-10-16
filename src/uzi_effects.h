#pragma once
#include <glm/glm.hpp>
#include <cstdint>

// Local cosmetic state only. Never changes projectiles, ammo or wire packets.
struct UziEffects {
    static constexpr int CAPACITY=48;
    static constexpr float EJECT_DELAY=.018f, CYCLE_TIME=.065f;
    static constexpr glm::vec3 PORT{.024f,.051f,-.002f};
    struct Casing {
        glm::vec3 pos{0},velocity{0},axis{0,1,0};
        float angle=0,spin=0,life=0;
        bool bounced=false;
    };
    Casing casings[CAPACITY]{};
    float shotAge=1;
    uint32_t seed=0x82bc53u;
    int next=0;
    bool pending=false;
    glm::mat4 shotMount{1};
    void clear();
    void shot(const glm::mat4& mount);
    void tick(float dt);
    float boltOffset() const;
private:
    float random();
    void eject();
};
