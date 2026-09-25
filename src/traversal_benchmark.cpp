#include "traversal_benchmark.h"
#include "renderer.h"
#include "camera.h"
#include "player_visual.h"
#include "hud.h"
#include "map.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

bool TraversalBenchmark::init(const Renderer& r) {
    const char* path=getenv("FPS_BENCH_ROUTE");
    if(!path) return true;
    for(const char* key:{"FPS_REF","FPS_POS","FPS_SHOT","FPS_BENCH","FPS_TPP","FPS_MEADOW_GPU"}) {
        if(getenv(key)) { fprintf(stderr,"[route] unset %s for route benchmarking\n",key); return false; }
    }
    if(strcmp(path,"1")==0) path="traversal.csv";
    output=fopen(path,"w");
    if(!output) { fprintf(stderr,"[route] cannot write %s\n",path); return false; }
    SDL_RaiseWindow(r.window);
    gProfiler.measureWork=true;
    const char* gpu=getenv("FPS_BENCH_GPU");
    bool timed=gpu && strcmp(gpu,"1")==0 && gGpuTiming.init();
    if(timed) printf("[route] WARNING: GPU queries can substantially slow this driver; use untimed runs for FPS comparisons\n");
    SDL_DisplayMode display{};
    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(r.window),&display);
    printf("[route] v1 training terrain-following camera; four stages, %d warmup + %d measured frames each\n",
           ROUTE_WARMUP,ROUTE_SAMPLES);
    printf("[route] fixed 1/60 s path steps; rendering/streaming test, no movement-collision or network proof\n");
    printf("[route] platform=%s cpu_count=%d ram_mb=%d GPU=%s GL=%s\n",
           SDL_GetPlatform(),SDL_GetCPUCount(),SDL_GetSystemRAM(),glGetString(GL_RENDERER),glGetString(GL_VERSION));
    printf("[route] drawable=%dx%d window=%dx%d display=%dx%d@%d swap=%d quality=%s gpu_timing=%s\n",
           r.fbW,r.fbH,r.width,r.height,display.w,display.h,display.refresh_rate,
           SDL_GL_GetSwapInterval(),gQuality.name,timed ? "on" : "unavailable/off");
#ifdef NDEBUG
    printf("[route] build=Release\n");
#else
    printf("[route] WARNING: build has assertions enabled; use Release for comparisons\n");
#endif
    printf("[route] no_reflect=%d no_veg=%d no_meadow=%d no_hud=%d output=%s\n",
           getenv("FPS_NOREFLECT")!=nullptr,getenv("FPS_NOVEG")!=nullptr,
           getenv("FPS_NOMEADOW")!=nullptr,getenv("FPS_NOHUD")!=nullptr,path);
    return true;
}

void TraversalBenchmark::collectGpu() {
    GpuFrameResult result;
    while(gGpuTiming.collect(result))
        if(result.sample>=0 && result.sample<count) samples[result.sample].gpu=result;
}

void TraversalBenchmark::beforeRender(Renderer& r,Camera& cam,ViewModel& vm,HudState& hud) {
    if(!enabled()) return;
    collectGpu();
    const int stage=frame/(ROUTE_WARMUP+ROUTE_SAMPLES);
    const int local=frame%(ROUTE_WARMUP+ROUTE_SAMPLES);
    if(local==0) printf("[route] warming %s\n",routeStageName(stage));
    if(local==ROUTE_WARMUP) printf("[route] measuring %s drawable=%dx%d focused=%d\n",
        routeStageName(stage),r.fbW,r.fbH,int((SDL_GetWindowFlags(r.window)&SDL_WINDOW_INPUT_FOCUS)!=0));
    int step=std::max(0,local-ROUTE_WARMUP);
    RoutePose p=routePose(stage,step);
    cam=Camera{};
    cam.eye={p.x,terrainHeight(p.x,p.z)+EYE_HEIGHT,p.z};
    cam.yaw=p.yaw; cam.pitch=p.pitch;
    cam.fov=glm::mix(HIP_FOV,SCOPE_8X_FOV,p.ads);
    vm=ViewModel{}; vm.adsT=p.ads; hud.adsT=p.ads;
    gWeaponId=stage==3 ? WEP_KAR98 : WEP_UZI;
    r.setTime(30.0f+stage*10.0f+step/60.0f);
    r.setAtmosphere(Renderer::ATMO_GOLDEN);
    if(local>=ROUTE_WARMUP) {
        auto& s=samples[count];
        s.pose=p; s.eyeY=cam.eye.y;
        s.width=r.fbW; s.height=r.fbH;
    }
    // Warm the driver query path too; do not charge first-query setup to sample 0.
    gGpuTiming.beginFrame(local>=ROUTE_WARMUP ? count : -1);
    renderStart=SDL_GetPerformanceCounter();
}

bool TraversalBenchmark::afterFrame(uint64_t start,const Renderer& renderer) {
    if(!enabled()) return false;
    if(frame%(ROUTE_WARMUP+ROUTE_SAMPLES)>=ROUTE_WARMUP) {
        auto& s=samples[count++];
        uint64_t end=SDL_GetPerformanceCounter();
        double ms=1000.0/SDL_GetPerformanceFrequency();
        s.frameMs=float((end-start)*ms);
        s.renderMs=float((end-renderStart)*ms);
        s.focused=(SDL_GetWindowFlags(renderer.window)&SDL_WINDOW_INPUT_FOCUS)!=0;
        for(int p=0;p<PASS_COUNT;++p) s.cpu[p]=gProfiler.passMsRaw[p];
        for(int b=0;b<BUILD_COUNT;++b) {
            s.buildMs[b]=gProfiler.buildMs[b]; s.builds[b]=gProfiler.builds[b];
        }
        s.trees=gVegStats.treesL0+gVegStats.treesL1+gVegStats.treesImp;
    }
    collectGpu();
    completed=++frame==ROUTE_STAGES*(ROUTE_WARMUP+ROUTE_SAMPLES);
    return completed;
}

void TraversalBenchmark::finish() {
    if(!enabled()) return;
    collectGpu();
    fprintf(output,"stage,sample,x,eye_y,z,yaw,pitch,ads,drawable_w,drawable_h,frame_ms,render_with_swap_ms,gpu_ms,trees,terrain_builds,terrain_build_ms,grass_builds,grass_build_ms,focused");
    for(int p=0;p<PASS_COUNT;++p) fprintf(output,",cpu_%s_ms,gpu_%s_ms",FrameProfiler::passName(RenderPass(p)),FrameProfiler::passName(RenderPass(p)));
    fprintf(output,"\n");
    for(int i=0;i<count;++i) {
        const auto& s=samples[i];
        fprintf(output,"%s,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%.4f,%.4f,%.4f,%d,%d,%.4f,%d,%.4f",
            routeStageName(i/ROUTE_SAMPLES),i%ROUTE_SAMPLES,s.pose.x,s.eyeY,s.pose.z,s.pose.yaw,s.pose.pitch,s.pose.ads,
            s.width,s.height,s.frameMs,s.renderMs,s.gpu.totalMs,s.trees,
            s.builds[BUILD_TERRAIN],s.buildMs[BUILD_TERRAIN],s.builds[BUILD_GRASS],s.buildMs[BUILD_GRASS]);
        fprintf(output,",%d",int(s.focused));
        for(int p=0;p<PASS_COUNT;++p) fprintf(output,",%.4f,%.4f",s.cpu[p],s.gpu.totalMs<0 ? -1 : s.gpu.passMs[p]);
        fprintf(output,"\n");
    }
    for(int stage=0;stage<ROUTE_STAGES;++stage) {
        int first=stage*ROUTE_SAMPLES,n=std::min(ROUTE_SAMPLES,count-first);
        if(n<=0) break;
        float sorted[ROUTE_SAMPLES];
        double sum=0,gpu=0,build=0; int gpuN=0,slow=0,worst=first;
        for(int i=0;i<n;++i) {
            const auto& s=samples[first+i]; sorted[i]=s.frameMs; sum+=s.frameMs;
            if(s.frameMs>samples[worst].frameMs) worst=first+i;
            if(s.frameMs>16.667f) ++slow;
            if(s.gpu.totalMs>=0) { gpu+=s.gpu.totalMs; ++gpuN; }
            build+=s.buildMs[BUILD_TERRAIN]+s.buildMs[BUILD_GRASS];
        }
        std::sort(sorted,sorted+n);
        auto percentile=[&](float q) { return sorted[std::max(0,int(ceilf(q*n))-1)]; };
        printf("[route] %s n=%d frame_ms avg=%.2f p50=%.2f p95=%.2f p99=%.2f max=%.2f over16.67=%d gpu_avg=%.2f gpu_n=%d build_avg=%.2f worst_sample=%d\n",
               routeStageName(stage),n,sum/n,percentile(.5f),percentile(.95f),percentile(.99f),sorted[n-1],slow,
               gpuN ? gpu/gpuN : -1,gpuN,build/n,worst-first);
    }
    int unfocused=0,resized=0;
    for(int i=0;i<count;++i) {
        unfocused+=!samples[i].focused;
        resized+=samples[i].width!=samples[0].width || samples[i].height!=samples[0].height;
    }
    if(unfocused || resized) printf("[route] WARNING: %d unfocused and %d resized samples; inspect CSV before comparing\n",unfocused,resized);
    if(ferror(output)) fprintf(stderr,"[route] ERROR writing CSV\n");
    if(fclose(output)!=0) fprintf(stderr,"[route] ERROR closing CSV\n");
    output=nullptr;
    printf("[route] %s: %d/%d samples; unavailable GPU samples use -1\n",completed ? "complete" : "interrupted",count,ROUTE_STAGES*ROUTE_SAMPLES);
    gGpuTiming.shutdown(); gProfiler.measureWork=false;
}
