#pragma once
#include "renderer.h"
#include "camera.h"
#include <cstdint>

struct ViewModel {
    float adsT       = 0.0f;  // 0 = hipfire pose, 1 = aimed
    float flashTimer = 0.0f;  // muzzle flash seconds remaining
    float recoilT    = 0.0f;  // recoil kick, 1 on fire, decays to 0
};

void drawViewModel(Renderer& r, const Camera& cam, const ViewModel& vm, uint8_t weaponId);
void drawPlayerSkeleton(Renderer& r, const glm::vec3& pos, float yaw, float pitch,
                        float lean, uint8_t weaponId, float crouch, float ads,
                        float phase, float amp, const glm::vec3& bodyCol, bool lowDetail = false);
