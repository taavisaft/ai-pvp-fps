#pragma once
#include "game.h"
#include "audio.h"
#include "map.h"
#include "lobby_ground.h"

// First two terrain banks. Existing worn dirt lanes use the gravel bank until
// separate dirt assets exist; box tops retain the neutral contact sound.
inline StepSurface stepSurface(float x, float z) {
    return gTerrainMode == TERRAIN_LOBBY && lobbyWear(x,z) >= 0.55f
        ? StepSurface::Gravel : StepSurface::Grass;
}
inline float stepSupport(const glm::vec3& pos, bool& solid) {
    float support = terrainHeight(pos.x, pos.z);
    solid = false;
    for (int i = 0; i < gMapBoxCount; ++i) {
        const Box& b = gMapBoxes[i];
        if (fabsf(pos.x-b.center.x) >= b.half.x+0.4f ||
            fabsf(pos.z-b.center.z) >= b.half.z+0.4f) continue;
        float top = b.center.y+b.half.y;
        if (top >= support && pos.y >= top-0.05f) {
            support = top;
            solid = true;
        }
    }
    return support;
}

// Allow small descending-slope gaps from discrete gravity integration, while
// rejecting upward jump motion. This preserves cadence over uneven terrain.
inline bool stepGrounded(float height, float velY) {
    return height >= -0.05f && height < 0.12f && velY <= 0.0f;
}

// Advance from collision-resolved simulation displacement, before network
// reconciliation. No steps from held keys, camera smoothing or server corrections.
struct Footsteps {
    float distance = 0.0f;
    bool walking = false;
    bool advance(float moved, bool grounded, bool alive, bool crouch, bool sprint) {
        if (!alive || !grounded || moved < 0.0001f || moved > 1.0f) {
            distance = 0.0f;
            walking = false;
            return false;
        }
        distance += moved;
        float stride = walking ? (crouch ? 1.25f : sprint ? 2.24f : 1.90f) : 0.25f;
        if (distance < stride) return false;
        distance -= stride;
        walking = true;
        return true;
    }
    void update(const Player& player, const InputState& input, const glm::vec3& before);
};
