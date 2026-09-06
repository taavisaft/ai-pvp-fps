#include "renderer.h"
#include <cstdio>

static int logicalW=1280, logicalH=720, pixelsW=2560, pixelsH=1440;
static int viewportW=0, viewportH=0, calls=0;
extern "C" void SDLCALL SDL_GetWindowSize(SDL_Window*,int* w,int* h) { *w=logicalW; *h=logicalH; }
extern "C" void SDLCALL SDL_GL_GetDrawableSize(SDL_Window*,int* w,int* h) { *w=pixelsW; *h=pixelsH; }
extern "C" void APIENTRY glViewport(GLint,GLint,GLsizei w,GLsizei h) { viewportW=w; viewportH=h; ++calls; }
static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)
int main() {
    Renderer r;
    r.refreshWindowSize();
    CHECK(r.fbW==2560 && r.fbH==1440 && viewportW==2560 && viewportH==1440);
    // Same logical window, different screen scale: old viewport caused cropping.
    pixelsW=1280; pixelsH=720; r.refreshWindowSize();
    CHECK(r.width==1280 && r.height==720 && r.fbW==1280 && r.fbH==720);
    CHECK(viewportW==1280 && viewportH==720 && calls==2);
    r.refreshWindowSize(); CHECK(calls==2);
    pixelsW=2560; pixelsH=1440; r.refreshWindowSize(); CHECK(calls==3 && r.fbW==2560);
    logicalW=900; logicalH=600; pixelsW=1800; pixelsH=1200; r.refreshWindowSize();
    CHECK(r.width==900 && r.height==600 && r.fbW==1800 && r.fbH==1200);
    logicalW=logicalH=pixelsW=pixelsH=0; r.refreshWindowSize();
    CHECK(r.width==900 && r.height==600 && r.fbW==1800 && r.fbH==1200 && calls==4);
    return failures ? 1 : 0;
}
