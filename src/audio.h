#pragma once
#include <cstdint>
#include <glm/vec3.hpp>

// SDL audio device + fixed voice mixer. WAV assets and procedural fallbacks
// are prepared at startup. Client-only.

enum SoundId {
    SND_SHOOT = 0,      // Uzi
    SND_SHOOT_GLOCK,    // Glock 19
    SND_SHOOT_KAR98,    // Kar98k
    SND_STEP,
    SND_DEATH,
    SND_RESPAWN,
    SND_RELOAD,
    SND_DRYFIRE,
    SND_STEP_GRASS_0,
    SND_STEP_GRAVEL_0 = SND_STEP_GRASS_0 + 8,
    SND_COUNT = SND_STEP_GRAVEL_0 + 8
};

// Map a weapon id (WeaponId in weapon.h) to its gunshot sound.
SoundId weaponShootSound(uint8_t weaponId);

struct Audio {
    bool init();       // open device, generate sound buffers; false if no audio
    void shutdown();
};

// Start a one-shot sound on a free voice. Safe to call every frame.
void audioPlay(SoundId id, float volume = 1.0f);

// Position-aware helper for future 3D audio. For now this applies only distance
// attenuation and forwards to the mono mixer.
void audioPlayAt(SoundId id, const glm::vec3& sourcePos, const glm::vec3& listenerPos,
                 const glm::vec3& listenerForward, float gain = 1.0f);

constexpr int FOOTSTEP_VARIANTS = 8;
enum class StepSurface { Grass, Gravel, Solid };
void audioPlayFootstep(StepSurface surface, float gain);
