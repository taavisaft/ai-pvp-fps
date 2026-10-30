#pragma once
#include "player_anim.h"
#include "renderer.h"
#include "camera.h"
#include <cstdint>

struct Player;

struct ViewModel {
    UziEffects uzi;
    float adsT       = 0.0f;  // 0 = hipfire pose, 1 = aimed
    float flashTimer = 0.0f;  // muzzle flash seconds remaining
    float recoilT    = 0.0f;  // recoil kick, 1 on fire, decays to 0
};

void drawPlayerSkeleton(Renderer& r, const glm::vec3& pos, float yaw, float pitch,
                        float lean, uint8_t weaponId, float crouch, float ads,
                        const PoseAnim& anim, const glm::vec3& bodyCol, bool lowDetail = false, float uziBolt = -.030f,
                        bool firstPerson = false);
glm::mat4 playerWeaponHold(const Player& player, float crouch, float ads, const PoseAnim& anim);

void fireUziCosmetics(ViewModel& vm,const Player& player,float crouch,const PoseAnim& anim);
