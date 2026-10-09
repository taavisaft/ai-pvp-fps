#include "footsteps.h"
#include "audio.h"

void Footsteps::update(const Player& player, const InputState& input, const glm::vec3& before) {
    bool solid;
    float support = stepSupport(player.pos, solid);
    float height = player.pos.y-support;
    bool grounded = stepGrounded(height, player.velY);
    float dx = player.pos.x-before.x, dz = player.pos.z-before.z;
    if (!advance(sqrtf(dx*dx+dz*dz), grounded, player.alive, input.crouch, input.sprint)) return;
    float gain = input.crouch ? 0.35f : input.sprint ? 0.85f : 0.65f;
    StepSurface surface = solid ? StepSurface::Solid : stepSurface(player.pos.x, player.pos.z);
    audioPlayFootstep(surface, gain);
}
