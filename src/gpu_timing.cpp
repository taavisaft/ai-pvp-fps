#include "gpu_timing.h"
#include <SDL.h>
#include <cstdio>

GpuTiming gGpuTiming;

bool GpuTiming::init() {
    auto gen=(Gen)SDL_GL_GetProcAddress("glGenQueries");
    del=(Del)SDL_GL_GetProcAddress("glDeleteQueries");
    counter=(Counter)SDL_GL_GetProcAddress("glQueryCounter");
    get=(Get)SDL_GL_GetProcAddress("glGetQueryObjectuiv");
    get64=(Get64)SDL_GL_GetProcAddress("glGetQueryObjectui64v");
    auto getBits=(Bits)SDL_GL_GetProcAddress("glGetQueryiv");
    begin=(Begin)SDL_GL_GetProcAddress("glBeginQuery");
    end=(End)SDL_GL_GetProcAddress("glEndQuery");
    if(!gen || !del || !get || !get64 || !getBits) return false;
    if(counter) getBits(GL_TIMESTAMP,GL_QUERY_COUNTER_BITS,&bits);
    if(bits<=0 && begin && end) {
        getBits(GL_TIME_ELAPSED,GL_QUERY_COUNTER_BITS,&bits);
        elapsed=true;
    }
    if(bits<=0) return false;
    for(auto& slot:slots) gen(STAMPS,slot.query);
    printf("[route] GPU %s counter: %d bits (asynchronous)\n",elapsed ? "elapsed" : "timestamp",bits);
    return true;
}

void GpuTiming::shutdown() {
    for(auto& slot:slots) if(slot.query[0]) del(STAMPS,slot.query);
    *this=GpuTiming{};
}

void GpuTiming::beginFrame(int sample) {
    if(!slots[0].query[0] || current>=0) return;
    for(int i=0;i<SLOTS;++i) {
        int n=(next+i)%SLOTS;
        auto& slot=slots[n];
        if(slot.pending) continue;
        for(bool& used:slot.used) used=false;
        slot.sample=sample; current=n; next=(n+1)%SLOTS;
        slot.last=elapsed ? 0 : 1;
        if(elapsed) begin(GL_TIME_ELAPSED,slot.query[0]);
        else counter(slot.query[0],GL_TIMESTAMP);
        return;
    }
}

void GpuTiming::beginPass(RenderPass pass) {
    if(current<0 || pass>=PASS_COUNT) return;
    slots[current].used[pass]=true;
    if(elapsed) { end(GL_TIME_ELAPSED); begin(GL_TIME_ELAPSED,slots[current].query[2+2*pass]); }
    else counter(slots[current].query[2+2*pass],GL_TIMESTAMP);
}

void GpuTiming::endPass(RenderPass pass) {
    if(current<0 || pass>=PASS_COUNT) return;
    if(elapsed) {
        end(GL_TIME_ELAPSED);
        slots[current].last=3+2*pass;
        begin(GL_TIME_ELAPSED,slots[current].query[3+2*pass]);
    } else counter(slots[current].query[3+2*pass],GL_TIMESTAMP);
}

void GpuTiming::endFrame() {
    if(current<0) return;
    if(elapsed) end(GL_TIME_ELAPSED);
    else counter(slots[current].query[1],GL_TIMESTAMP);
    slots[current].pending=true; current=-1;
}

bool GpuTiming::collect(GpuFrameResult& result) {
    for(auto& slot:slots) {
        if(!slot.pending) continue;
        GLuint ready=0;
        get(slot.query[slot.last],GL_QUERY_RESULT_AVAILABLE,&ready);
        if(!ready) continue;
        const uint64_t mask=bits>=64 ? UINT64_MAX : (uint64_t(1)<<bits)-1;
        auto duration=[&](int start,int stop) {
            uint64_t a=0,b=0;
            if(!elapsed) get64(slot.query[start],GL_QUERY_RESULT,&a);
            get64(slot.query[elapsed ? start : stop],GL_QUERY_RESULT,&b);
            return float((b-a)&mask)*.000001f;
        };
        result=GpuFrameResult{};
        result.sample=slot.sample; result.totalMs=duration(0,1);
        for(int p=0;p<PASS_COUNT;++p) if(slot.used[p]) {
            result.passMs[p]=duration(2+2*p,3+2*p);
            // Elapsed fallback includes every gap as well as each named pass.
            if(elapsed) result.totalMs+=result.passMs[p]+duration(3+2*p,0);
        }
        slot.pending=false;
        return true;
    }
    return false;
}
