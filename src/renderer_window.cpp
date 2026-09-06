#include "renderer.h"

void Renderer::refreshWindowSize() {
    // Moving between Retina and standard-DPI screens can change backing pixels
    // without changing logical window dimensions. Query after SDL pumps events.
    int w=0, h=0, drawableW=0, drawableH=0;
    SDL_GetWindowSize(window,&w,&h);
    SDL_GL_GetDrawableSize(window,&drawableW,&drawableH);
    if(w>0 && h>0) { width=w; height=h; }
    // Minimized/transitional windows may report zero; retain the last valid size.
    if(drawableW<=0 || drawableH<=0) return;
    if(drawableW==fbW && drawableH==fbH) return;
    fbW=drawableW; fbH=drawableH;
    glViewport(0,0,fbW,fbH);
}
