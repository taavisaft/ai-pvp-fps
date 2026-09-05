#pragma once
#include "physics.h"

extern double serverTime;
void recordSnapshot(const GameState& game);
void recordStateTime(uint32_t seq);
float rewindForShot(uint32_t seq, uint8_t fraction);
bool rewindLookup(const void* ctx, int pid, float rewindSec,
                  glm::vec3& pos, bool& crouched, float& yaw, float& pitch,
                  float& lean, bool& ads, uint8_t& weaponId, bool& alive);
