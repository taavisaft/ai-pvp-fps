#include "perf.h"
#include "renderer.h"
#include "camera.h"
#include "game.h"
#include "map.h"
#include "terrain.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>

QualitySettings gQuality;
VegDrawStats    gVegStats;
FrameProfiler   gProfiler;

static QualitySettings makeQuality(QualityTier tier) {
    QualitySettings q;
    q.tier = tier;
    switch (tier) {
    case QUALITY_LOW:
        q.name                  = "low";
        q.shadowSize            = 1024;
        q.treeFade0             = 48.0f;
        q.treeL0End             = 58.0f;
        q.treeFade1             = 220.0f;
        q.treeL1End             = 260.0f;
        q.treeImpFade           = 900.0f;
        q.treeImpEnd            = 1000.0f;
        q.treeShadowRange       = 55.0f;
        q.bushFade              = 80.0f;
        q.bushEnd               = 110.0f;
        q.bushShadowRange       = 28.0f;
        q.grassEnabled          = false;
        q.terrainBuildsPerFrame = 0;   // coarse-only upgrades, no hitches
        break;
    case QUALITY_HIGH:
        q.name                  = "high";
        q.shadowSize            = 2048;
        q.treeFade0             = 58.0f;
        q.treeL0End             = 85.0f;
        q.treeFade1             = 300.0f;
        q.treeL1End             = 360.0f;
        q.treeImpFade           = 1500.0f;
        q.treeImpEnd            = 1700.0f;
        q.treeShadowRange       = 100.0f;
        q.bushFade              = 130.0f;
        q.bushEnd               = 180.0f;
        q.bushShadowRange       = 50.0f;
        q.grassEnabled          = false;  // still parked until blade pass is reworked
        q.terrainBuildsPerFrame = 2;
        break;
    default:
        q.name                  = "medium";
        q.shadowSize            = 2048;
        q.treeFade0             = 58.0f;
        q.treeL0End             = 72.0f;
        q.treeFade1             = 280.0f;
        q.treeL1End             = 320.0f;
        q.treeImpFade           = 1350.0f;
        q.treeImpEnd            = 1500.0f;
        q.treeShadowRange       = 85.0f;
        q.bushFade              = 120.0f;
        q.bushEnd               = 150.0f;
        q.bushShadowRange       = 40.0f;
        q.grassEnabled          = false;
        q.terrainBuildsPerFrame = 1;
        break;
    }
    return q;
}

QualitySettings qualityFromEnv() {
    const char* q = getenv("FPS_QUALITY");
    if (!q) return makeQuality(QUALITY_MED);
    if (strcmp(q, "low") == 0 || strcmp(q, "0") == 0) return makeQuality(QUALITY_LOW);
    if (strcmp(q, "high") == 0 || strcmp(q, "2") == 0) return makeQuality(QUALITY_HIGH);
    return makeQuality(QUALITY_MED);
}

void applyQuality(Renderer& r, const QualitySettings& q) {
    gQuality = q;
    r.setShadowMapSize(q.shadowSize);
    r.taigaTerrain.maxBuildsPerFrame = q.terrainBuildsPerFrame;
    r.veg.applyQuality(q);
    printf("[quality] tier=%s shadow=%d treeImpEnd=%.0f grass=%s terrainBuilds=%d/frame\n",
           q.name, q.shadowSize, q.treeImpEnd, q.grassEnabled ? "on" : "off",
           q.terrainBuildsPerFrame);
}

void FrameProfiler::configureFromEnv() {
    showHud = getenv("FPS_PERF") != nullptr;
}

void FrameProfiler::beginFrame() {
    frameStart = SDL_GetPerformanceCounter();
}

void FrameProfiler::endFrame() {
    Uint64 freq = SDL_GetPerformanceFrequency();
    totalMsRaw = (float)(SDL_GetPerformanceCounter() - frameStart) * 1000.0f / (float)freq;
}

void FrameProfiler::beginPass(RenderPass p) {
    passStart = SDL_GetPerformanceCounter();
    (void)p;
}

void FrameProfiler::endPass(RenderPass p) {
    if (p >= PASS_COUNT) return;
    Uint64 freq = SDL_GetPerformanceFrequency();
    float ms = (float)(SDL_GetPerformanceCounter() - passStart) * 1000.0f / (float)freq;
    passMsRaw[p] = ms;
    passMs[p] = passMs[p] <= 0.0f ? ms : passMs[p] + (ms - passMs[p]) * 0.12f;
}

const char* FrameProfiler::passName(RenderPass p) {
    static const char* names[] = {"shadow", "sky", "world", "water", "hud"};
    return (p < PASS_COUNT) ? names[p] : "?";
}

static const RefCameraPreset kRefCameras[] = {
    // Shore: wade line on the Baltic, looking west into the sea.
    {"shore",   {-720.0f, 0.0f, 180.0f},  -95.0f, -4.0f,  Renderer::ATMO_CLEAR},
    // Bog meadow: primary hilltop spawn area, dense olive ground + treeline ring.
    {"bog",     {-301.0f, 0.0f, 527.0f}, -130.0f, -8.0f,  Renderer::ATMO_OVERCAST},
    // Forest interior: mid-taiga spruce stand.
    {"forest",  {120.0f, 0.0f, -240.0f},  35.0f,  -2.0f,  Renderer::ATMO_CLEAR},
    // Ridge vista: high ground looking east toward the mountain backdrop.
    {"ridge",   {-80.0f, 0.0f, 640.0f},   55.0f,  -6.0f,  Renderer::ATMO_CLEAR},
    // Golden hour: same ridge, warm light.
    {"golden",  {-80.0f, 0.0f, 640.0f},   55.0f,  -6.0f,  Renderer::ATMO_GOLDEN},
};

int refCameraCount() { return (int)(sizeof(kRefCameras) / sizeof(kRefCameras[0])); }

const RefCameraPreset& refCameraPreset(int i) {
    if (i < 0) i = 0;
    if (i >= refCameraCount()) i = refCameraCount() - 1;
    return kRefCameras[i];
}

const RefCameraPreset* refCameraFromEnv() {
    const char* v = getenv("FPS_REF");
    if (!v || !v[0]) return nullptr;
    if (strcmp(v, "all") == 0) return &kRefCameras[0];
    for (int i = 0; i < refCameraCount(); i++)
        if (strcmp(v, kRefCameras[i].name) == 0) return &kRefCameras[i];
    // Numeric index fallback: FPS_REF=0 .. n-1
    char* end = nullptr;
    long idx = strtol(v, &end, 10);
    if (end != v && idx >= 0 && idx < refCameraCount())
        return &kRefCameras[(int)idx];
    fprintf(stderr, "[ref] unknown FPS_REF=%s (try shore|bog|forest|ridge|golden)\n", v);
    return nullptr;
}

void applyRefCamera(Camera& cam, const RefCameraPreset& ref) {
    float gy = terrainHeight(ref.feet.x, ref.feet.z);
    cam.yaw   = ref.yaw;
    cam.pitch = ref.pitch;
    cam.lean  = 0.0f;
    cam.eye   = {ref.feet.x, gy + EYE_HEIGHT, ref.feet.z};
}
