#pragma once
#include "benchmark_route.h"
#include "gpu_timing.h"
#include <cstdio>

struct Renderer;
struct Camera;
struct ViewModel;
struct HudState;

class TraversalBenchmark {
public:
    bool init(const Renderer& renderer);
    bool enabled() const { return output!=nullptr; }
    void beforeRender(Renderer& renderer,Camera& camera,ViewModel& vm,HudState& hud);
    bool afterFrame(uint64_t start,const Renderer& renderer);
    void finish();
private:
    struct Sample {
        RoutePose pose{};
        float frameMs=0, renderMs=0, cpu[PASS_COUNT]{};
        float buildMs[BUILD_COUNT]{};
        int builds[BUILD_COUNT]{};
        GpuFrameResult gpu{};
        int width=0,height=0,trees=0;
        bool focused=false;
        float eyeY=0;
    } samples[ROUTE_STAGES*ROUTE_SAMPLES];
    FILE* output=nullptr;
    int frame=0,count=0;
    uint64_t renderStart=0;
    bool completed=false;
    void collectGpu();
};
