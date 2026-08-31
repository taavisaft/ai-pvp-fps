#pragma once
#include <cstdint>
#include <glm/glm.hpp>

struct Renderer;
struct Camera;

// Runtime quality tier — set once at startup from FPS_QUALITY=low|medium|high.
enum QualityTier : uint8_t { QUALITY_LOW = 0, QUALITY_MED, QUALITY_HIGH, QUALITY_COUNT };

struct QualitySettings {
    QualityTier tier                = QUALITY_MED;
    const char* name                = "medium";
    int         shadowSize          = 2048;
    float       treeFade0           = 58.0f;
    float       treeL0End           = 72.0f;
    float       treeFade1           = 280.0f;
    float       treeL1End           = 320.0f;
    float       treeImpFade         = 1350.0f;
    float       treeImpEnd          = 1500.0f;
    float       treeShadowRange     = 85.0f;
    float       bushFade            = 120.0f;
    float       bushEnd             = 150.0f;
    float       bushShadowRange     = 40.0f;
    bool        grassEnabled        = false;
    int         terrainBuildsPerFrame = 1;
};

extern QualitySettings gQuality;

// Per-frame vegetation instance counts (reset at the start of each lit veg pass).
struct VegDrawStats {
    int treesL0  = 0;
    int treesL1  = 0;
    int treesImp = 0;
    int bushes   = 0;
    void reset() { treesL0 = treesL1 = treesImp = bushes = 0; }
};

extern VegDrawStats gVegStats;

QualitySettings qualityFromEnv();
void          applyQuality(Renderer& r, const QualitySettings& q);

enum RenderPass : uint8_t {
    PASS_SHADOW = 0,
    PASS_SKY,
    PASS_WORLD,
    PASS_WATER,
    PASS_HUD,
    PASS_COUNT
};

struct FrameProfiler {
    float passMs[PASS_COUNT]     = {};  // exponential smooth
    float passMsRaw[PASS_COUNT]  = {};  // last frame
    float totalMsRaw             = 0.0f;
    bool  showHud                = false;  // FPS_PERF=1

    void configureFromEnv();
    void beginFrame();
    void endFrame();
    void beginPass(RenderPass p);
    void endPass(RenderPass p);

    static const char* passName(RenderPass p);

private:
    uint64_t passStart  = 0;
    uint64_t frameStart = 0;
};

extern FrameProfiler gProfiler;

// Fixed Paldiski viewpoints for visual regression (FPS_REF=<name> or FPS_REF=all).
struct RefCameraPreset {
    const char* name;
    glm::vec3   feet;   // player feet position
    float       yaw;
    float       pitch;
    int         atmo;   // Renderer::Atmo preset index
};

int                    refCameraCount();
const RefCameraPreset& refCameraPreset(int i);
const RefCameraPreset* refCameraFromEnv();   // nullptr = player control
void                   applyRefCamera(Camera& cam, const RefCameraPreset& ref);
