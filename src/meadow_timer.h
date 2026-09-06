#pragma once
#include "gl_loader.h"
#include <SDL.h>
#include <cstdint>
#include <cstdio>

// Optional delayed GPU timing. Unavailable results are skipped, never waited on.
struct MeadowTimer {
    using Gen = void (APIENTRY*)(GLsizei,GLuint*);
    using Del = void (APIENTRY*)(GLsizei,const GLuint*);
    using Begin = void (APIENTRY*)(GLenum,GLuint);
    using End = void (APIENTRY*)(GLenum);
    using Available = void (APIENTRY*)(GLuint,GLenum,GLuint*);
    using Result = void (APIENTRY*)(GLuint,GLenum,uint64_t*);
    Gen gen=nullptr; Del del=nullptr; Begin beginQuery=nullptr; End endQuery=nullptr;
    Available available=nullptr; Result result=nullptr;
    GLuint ids[4]{};
    bool pending[4]{}, running=false;
    int slot=0, received=0, samples=0;
    double totalNs=0;
    void init() {
        if (gen) return;
        gen=(Gen)SDL_GL_GetProcAddress("glGenQueries");
        del=(Del)SDL_GL_GetProcAddress("glDeleteQueries");
        beginQuery=(Begin)SDL_GL_GetProcAddress("glBeginQuery");
        endQuery=(End)SDL_GL_GetProcAddress("glEndQuery");
        available=(Available)SDL_GL_GetProcAddress("glGetQueryObjectuiv");
        result=(Result)SDL_GL_GetProcAddress("glGetQueryObjectui64v");
        if(gen && del && beginQuery && endQuery && available && result) gen(4,ids);
    }
    void begin() {
        if (!ids[0]) return;
        if(pending[slot]) {
            GLuint ready=0; available(ids[slot],GL_QUERY_RESULT_AVAILABLE,&ready);
            if(!ready) return;
            uint64_t ns=0; result(ids[slot],GL_QUERY_RESULT,&ns);
            if(++received>300) { totalNs+=(double)ns; ++samples; }
            pending[slot]=false;
        }
        beginQuery(GL_TIME_ELAPSED,ids[slot]); running=true;
    }
    void end() {
        if(!running) return;
        endQuery(GL_TIME_ELAPSED); pending[slot]=true; slot=(slot+1)%4; running=false;
    }
    void destroy(const char* label) {
        if(samples) std::printf("[meadow GPU] %s mean %.3f ms (%d delayed samples after warmup)\n",label,totalNs/samples/1e6,samples);
        if(ids[0]) del(4,ids);
        *this=MeadowTimer{};
    }
};
