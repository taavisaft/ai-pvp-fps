#pragma once
#include <glm/vec3.hpp>

// Tiny procedural sound engine: SDL audio device + a fixed voice mixer.
// All sounds are synthesized in code at init (no asset files). Client-only.

enum SoundId {
    SND_SHOOT = 0,
    SND_JUMP,
    SND_STEP,
    SND_DEATH,
    SND_RESPAWN,
    SND_RELOAD,
    SND_DRYFIRE,
    SND_COUNT
};

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
