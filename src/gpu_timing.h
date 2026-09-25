#pragma once
#include "gl_loader.h"
#include "perf.h"
#include <cstdint>

struct GpuFrameResult {
    int sample=-1;
    float totalMs=-1, passMs[PASS_COUNT]{};
};

// Prefer timestamps; the elapsed-query fallback uses separate, nonnested
// intervals. Results retain their sample ID; a busy ring skips timing, never waits.
class GpuTiming {
public:
    bool init();
    void shutdown();
    void beginFrame(int sample);
    void beginPass(RenderPass pass);
    void endPass(RenderPass pass);
    void endFrame();
    bool collect(GpuFrameResult& result);
private:
    using Gen=void(APIENTRY*)(GLsizei,GLuint*);
    using Del=void(APIENTRY*)(GLsizei,const GLuint*);
    using Counter=void(APIENTRY*)(GLuint,GLenum);
    using Get=void(APIENTRY*)(GLuint,GLenum,GLuint*);
    using Get64=void(APIENTRY*)(GLuint,GLenum,uint64_t*);
    using Bits=void(APIENTRY*)(GLenum,GLenum,GLint*);
    using Begin=void(APIENTRY*)(GLenum,GLuint);
    using End=void(APIENTRY*)(GLenum);
    static constexpr int SLOTS=16, STAMPS=2+2*PASS_COUNT;
    struct Slot {
        GLuint query[STAMPS]{};
        bool used[PASS_COUNT]{}, pending=false;
        int sample=-1,last=1;
    } slots[SLOTS];
    Del del=nullptr; Counter counter=nullptr; Get get=nullptr; Get64 get64=nullptr;
    Begin begin=nullptr; End end=nullptr;
    bool elapsed=false;
    int current=-1, next=0, bits=0;
};
extern GpuTiming gGpuTiming;
