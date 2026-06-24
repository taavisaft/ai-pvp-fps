#include "audio.h"
#include "weapon.h"
#include <SDL.h>
#include <glm/geometric.hpp>
#include <vector>
#include <cmath>
#include <cstdlib>

namespace {

constexpr int   SR        = 44100;   // sample rate, mono float32
constexpr int   MAX_VOICE = 16;      // simultaneous sounds

struct Voice {
    const float* data   = nullptr;
    int          len    = 0;
    int          pos    = 0;
    float        vol    = 1.0f;
    bool         active = false;
};

SDL_AudioDeviceID gDevice = 0;
std::vector<float> gSounds[SND_COUNT];   // generated once at init
Voice              gVoices[MAX_VOICE];

float randf() { return (float)rand() / (float)RAND_MAX; }
float noise() { return randf() * 2.0f - 1.0f; }

// Mix all active voices into the output buffer (called on the audio thread).
void mixCallback(void*, Uint8* stream, int bytes) {
    float* out = (float*)stream;
    int    n   = bytes / (int)sizeof(float);
    for (int i = 0; i < n; i++) out[i] = 0.0f;

    for (Voice& v : gVoices) {
        if (!v.active) continue;
        for (int i = 0; i < n && v.pos < v.len; i++)
            out[i] += v.data[v.pos++] * v.vol;
        if (v.pos >= v.len) v.active = false;
    }
    for (int i = 0; i < n; i++) {            // soft clamp
        if (out[i] >  1.0f) out[i] =  1.0f;
        if (out[i] < -1.0f) out[i] = -1.0f;
    }
}

// --- synthesis ------------------------------------------------------------

std::vector<float> makeBuffer(float seconds) {
    return std::vector<float>((size_t)(seconds * SR), 0.0f);
}

// Punchy gunshot: noise burst + low thump, fast decay, one-pole low-pass.
void genShoot(std::vector<float>& b) {
    b = makeBuffer(0.18f);
    float lp = 0.0f;
    for (size_t i = 0; i < b.size(); i++) {
        float t   = (float)i / SR;
        float env = expf(-t * 24.0f);
        lp += (noise() - lp) * 0.5f;                 // soften the hiss
        float thump = sinf(2.0f * (float)M_PI * 95.0f * t) * expf(-t * 32.0f);
        b[i] = (lp * 0.7f + thump * 0.5f) * env * 0.7f;
    }
}

// Soft low thud.
void genStep(std::vector<float>& b) {
    b = makeBuffer(0.08f);
    float lp = 0.0f;
    for (size_t i = 0; i < b.size(); i++) {
        float t   = (float)i / SR;
        float env = expf(-t * 45.0f);
        lp += (noise() - lp) * 0.15f;                // low-frequency
        float body = sinf(2.0f * (float)M_PI * 130.0f * t);
        b[i] = (lp * 0.5f + body * 0.5f) * env * 0.25f;
    }
}

// Descending tone for death.
void genDeath(std::vector<float>& b) {
    b = makeBuffer(0.5f);
    for (size_t i = 0; i < b.size(); i++) {
        float t   = (float)i / SR;
        float f   = 420.0f - 340.0f * (t / 0.5f);
        float env = expf(-t * 4.0f);
        b[i] = sinf(2.0f * (float)M_PI * f * t) * env * 0.4f;
    }
}

// Two mechanical clacks — magazine out, magazine in.
void genReload(std::vector<float>& b) {
    b = makeBuffer(0.35f);
    float lp = 0.0f;
    for (size_t i = 0; i < b.size(); i++) {
        float t  = (float)i / SR;
        float e1 = t >= 0.0f  ? expf(-(t - 0.0f)  * 55.0f) : 0.0f;
        float e2 = t >= 0.16f ? expf(-(t - 0.16f) * 55.0f) : 0.0f;
        lp += (noise() - lp) * 0.4f;
        b[i] = lp * (e1 + e2) * 0.4f;
    }
}

// Short click when the trigger is pulled on an empty magazine.
void genDryFire(std::vector<float>& b) {
    b = makeBuffer(0.04f);
    for (size_t i = 0; i < b.size(); i++) {
        float t = (float)i / SR;
        b[i] = noise() * expf(-t * 120.0f) * 0.3f;
    }
}

// Rising tone for respawn.
void genRespawn(std::vector<float>& b) {
    b = makeBuffer(0.3f);
    for (size_t i = 0; i < b.size(); i++) {
        float t   = (float)i / SR;
        float f   = 200.0f + 320.0f * (t / 0.3f);
        float env = sinf((float)M_PI * (t / 0.3f));  // fade in and out
        b[i] = sinf(2.0f * (float)M_PI * f * t) * env * 0.35f;
    }
}

// --- WAV loading ----------------------------------------------------------

// Load a WAV file and convert it to the mixer's format (mono float32 @ SR).
// Returns false if the file is missing or the format can't be converted, so
// callers can fall back to a synthesized buffer.
bool loadWav(const char* path, std::vector<float>& out) {
    SDL_AudioSpec spec{};
    Uint8*        wav  = nullptr;
    Uint32        wlen = 0;
    if (!SDL_LoadWAV(path, &spec, &wav, &wlen)) return false;

    SDL_AudioCVT cvt;
    int r = SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq,
                              AUDIO_F32SYS, 1, SR);
    if (r < 0) { SDL_FreeWAV(wav); return false; }   // unsupported conversion

    int mult = cvt.len_mult > 0 ? cvt.len_mult : 1;
    std::vector<Uint8> buf((size_t)wlen * mult);
    memcpy(buf.data(), wav, wlen);
    SDL_FreeWAV(wav);

    cvt.buf = buf.data();
    cvt.len = (int)wlen;
    if (SDL_ConvertAudio(&cvt) < 0) return false;

    int n = cvt.len_cvt / (int)sizeof(float);
    const float* f = (const float*)buf.data();
    out.assign(f, f + n);
    return true;
}

} // namespace

bool Audio::init() {
    SDL_AudioSpec want{}, have{};
    want.freq     = SR;
    want.format   = AUDIO_F32SYS;
    want.channels = 1;
    want.samples  = 512;
    want.callback = mixCallback;

    gDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (gDevice == 0) {
        fprintf(stderr, "audio: %s (running silent)\n", SDL_GetError());
        return false;
    }

    // Real recorded gunshots; fall back to the synth if an asset is missing.
    if (!loadWav("sounds/weapons/uzi.wav", gSounds[SND_SHOOT]))
        genShoot(gSounds[SND_SHOOT]);
    if (!loadWav("sounds/weapons/glock.wav", gSounds[SND_SHOOT_GLOCK]))
        genShoot(gSounds[SND_SHOOT_GLOCK]);
    genStep(gSounds[SND_STEP]);
    genDeath(gSounds[SND_DEATH]);
    genRespawn(gSounds[SND_RESPAWN]);
    genReload(gSounds[SND_RELOAD]);
    genDryFire(gSounds[SND_DRYFIRE]);

    SDL_PauseAudioDevice(gDevice, 0);   // start playback
    return true;
}

void Audio::shutdown() {
    if (gDevice) SDL_CloseAudioDevice(gDevice);
    gDevice = 0;
}

void audioPlay(SoundId id, float volume) {
    if (gDevice == 0 || id < 0 || id >= SND_COUNT) return;
    const std::vector<float>& buf = gSounds[id];
    if (buf.empty()) return;

    SDL_LockAudioDevice(gDevice);
    Voice* slot = nullptr;
    for (Voice& v : gVoices) {           // reuse a free voice, else steal the oldest-progress one
        if (!v.active) { slot = &v; break; }
    }
    if (!slot) {
        slot = &gVoices[0];
        for (Voice& v : gVoices)
            if (v.pos > slot->pos) slot = &v;
    }
    slot->data   = buf.data();
    slot->len    = (int)buf.size();
    slot->pos    = 0;
    slot->vol    = volume;
    slot->active = true;
    SDL_UnlockAudioDevice(gDevice);
}

SoundId weaponShootSound(uint8_t weaponId) {
    return weaponId == WEP_GLOCK19 ? SND_SHOOT_GLOCK : SND_SHOOT;
}

void audioPlayAt(SoundId id, const glm::vec3& sourcePos, const glm::vec3& listenerPos,
                 const glm::vec3& listenerForward, float gain) {
    (void)listenerForward;  // reserved for stereo panning in a future pass
    float d = glm::length(sourcePos - listenerPos);
    float att = 1.0f - d / 60.0f;
    if (att <= 0.08f) return;
    if (att > 1.0f) att = 1.0f;
    audioPlay(id, att * gain);
}
