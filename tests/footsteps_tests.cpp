#include "footsteps.h"
#include "footstep_sound.h"
#include "audio.h"
#include "physics.h"
#include <SDL.h>
#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while (0)

int main() {
    setMap(MAP_LOBBY);
    CHECK(stepSurface(0,30) == StepSurface::Grass);
    CHECK(stepSurface(12,0) == StepSurface::Gravel);
    CHECK(stepSurface(0,19) == StepSurface::Gravel);
    CHECK(stepSurface(0,23) == StepSurface::Grass);
    bool solid;
    CHECK(stepSupport({0,terrainHeight(0,30),30},solid) == terrainHeight(0,30));
    CHECK(!solid);
    CHECK(fabsf(stepSupport({6,1.6f,8},solid)-1.6f) < .001f);
    CHECK(solid);
    setMap(MAP_PALDISKI);
    CHECK(stepSurface(FOREST_SITE_X,FOREST_SITE_Z) == StepSurface::Grass);

    CHECK(stepGrounded(.08f,-.5f)); // downhill gravity gaps
    CHECK(!stepGrounded(.08f,2.0f)); // jump takeoff
    CHECK(!stepGrounded(.2f,-1.0f)); // falling above support
    // Actual physics on a sloping meadow must retain a useful footstep cadence.
    setMap(MAP_LOBBY);
    Player downhill;
    downhill.pos = {100,terrainHeight(100,100),100};
    InputState downhillInput{};
    downhillInput.w = true;
    downhillInput.yaw = terrainHeight(120,100) < terrainHeight(80,100) ? 0 : 180;
    Footsteps downhillSteps;
    int contacts = 0;
    for (int i=0; i<240; ++i) {
        glm::vec3 before = downhill.pos;
        movePlayer(downhill,downhillInput,1.0f/60.0f);
        float dx=downhill.pos.x-before.x, dz=downhill.pos.z-before.z;
        float floor=stepSupport(downhill.pos,solid);
        contacts += downhillSteps.advance(sqrtf(dx*dx+dz*dz),
            stepGrounded(downhill.pos.y-floor,downhill.velY),true,false,false);
    }
    CHECK(contacts >= 8 && contacts <= 12);

    // Equivalent simulation paths produce identical cadence at 30 and 120 Hz.
    for (bool sprint : {false,true}) {
        int counts[2]{};
        for (int i = 0; i < 2; ++i) {
            Footsteps steps;
            int hz = i ? 120 : 30;
            for (int tick = 0; tick < hz*4; ++tick)
                counts[i] += steps.advance((sprint ? 8.0f : 5.0f)/hz,true,true,false,sprint);
        }
        CHECK(counts[0] == counts[1]);
        CHECK(counts[0] > 9 && counts[0] < 16);
    }
    Footsteps steps;
    CHECK(!steps.advance(.1f,true,true,false,false)); // no immediate key-triggered step
    CHECK(steps.advance(.16f,true,true,false,false));
    for (int i = 0; i < 100; ++i) CHECK(!steps.advance(0,true,true,false,false)); // blocked
    CHECK(!steps.advance(.3f,false,true,false,false)); // airborne
    CHECK(!steps.advance(.3f,true,false,false,false)); // dead
    CHECK(!steps.advance(10,true,true,false,false)); // teleport
    CHECK(!steps.advance(.1f,true,true,true,true));
    CHECK(steps.advance(.16f,true,true,true,true));
    int crouchSteps = 0;
    for (int i = 0; i < 300; ++i) crouchSteps += steps.advance(.025f,true,true,true,true);
    CHECK(crouchSteps == 6); // crouch takes precedence over sprint

    // Fallbacks have bounded, finite samples and genuinely different banks/takes.
    std::vector<float> grass, gravel, alternate;
    buildFootstepFallback(grass,false,0);
    buildFootstepFallback(gravel,true,0);
    buildFootstepFallback(alternate,false,1);
    CHECK(grass != alternate && grass.size() != gravel.size());
    for (const auto* bank : {&grass,&gravel,&alternate}) {
        float peak = 0;
        for (float v : *bank) { CHECK(std::isfinite(v) && fabsf(v) < 1); peak = fmaxf(peak,fabsf(v)); }
        CHECK(peak > .01f && fabsf(bank->back()) < .001f);
    }
    // Exercise actual startup, bank playback and shutdown without hardware audio.
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    CHECK(SDL_Init(SDL_INIT_AUDIO) == 0);
    Audio audio;
    bool initialized = audio.init();
    CHECK(initialized);
    if (initialized) {
        for (int i = 0; i < 32; ++i) {
            audioPlayFootstep(StepSurface::Grass,.65f);
            audioPlayFootstep(StepSurface::Gravel,.85f);
        }
        SDL_Delay(40);
        audio.shutdown();
    }
    SDL_Quit();
    return failures ? 1 : 0;
}
