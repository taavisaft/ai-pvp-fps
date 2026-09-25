#include "settings_menu.h"
#include "camera.h"
#include "input.h"
#include "renderer.h"
#include <cmath>
#include <cstdio>

void SettingsMenu::load(Camera& cam) {
    char* directory=SDL_GetPrefPath("AI-PvP", "FPS");
    if(!directory) return;
    int length=snprintf(path,sizeof(path),"%ssettings.txt",directory);
    SDL_free(directory);
    if(length<0 || length>=int(sizeof(path))) { path[0]=0; return; }
    FILE* file=fopen(path,"r");
    if(!file) return;
    float sensitivity=1,ads=1; int invert=0;
    if(fscanf(file,"%f %f %d",&sensitivity,&ads,&invert)==3 &&
       std::isfinite(sensitivity) && std::isfinite(ads)) {
        cam.lookSensitivity=glm::clamp(sensitivity,.1f,3.0f);
        cam.adsSensitivity=glm::clamp(ads,.1f,2.0f);
        cam.invertY=invert!=0;
    }
    fclose(file);
}

void SettingsMenu::save(const Camera& cam) {
    FILE* file=path[0] ? fopen(path,"w") : nullptr;
    saveFailed=!file;
    if(!file) { std::fprintf(stderr,"Settings could not be saved: %s\n",path); return; }
    bool ok=fprintf(file,"%.2f %.2f %d\n",cam.lookSensitivity,cam.adsSensitivity,int(cam.invertY))>0;
    saveFailed=fclose(file)!=0 || !ok;
}

void SettingsMenu::setOpen(bool value) {
    open=value;
    SDL_SetRelativeMouseMode(value ? SDL_FALSE : SDL_TRUE);
}

void SettingsMenu::event(const SDL_Event& e,Camera& cam,FrameInput& input) {
    if(e.type==SDL_QUIT) { input.quit=true; return; }
    int direction=0; bool activate=false;
    if(e.type==SDL_KEYDOWN) {
        auto key=e.key.keysym.sym;
        if(key==SDLK_ESCAPE && !e.key.repeat) { save(cam); setOpen(false); return; }
        if(key==SDLK_UP) selected=(selected+6)%7;
        if(key==SDLK_DOWN) selected=(selected+1)%7;
        if(key==SDLK_LEFT) direction=-1;
        if(key==SDLK_RIGHT) direction=1;
        activate=(key==SDLK_RETURN || key==SDLK_SPACE) && !e.key.repeat;
    }
    if(e.type==SDL_MOUSEMOTION || e.type==SDL_MOUSEBUTTONDOWN) {
        SDL_Window* window=SDL_GetWindowFromID(e.type==SDL_MOUSEMOTION ? e.motion.windowID : e.button.windowID);
        int w=0,h=0; if(window) SDL_GetWindowSize(window,&w,&h);
        if(w<=0 || h<=0) return;
        int mx=e.type==SDL_MOUSEMOTION ? e.motion.x : e.button.x;
        int my=e.type==SDL_MOUSEMOTION ? e.motion.y : e.button.y;
        float x=2.0f*mx/w-1, y=1-2.0f*my/h;
        int row=int(std::floor((.335f-y)/.105f));
        if(x<-.58f || x>.58f || row<0 || row>=7) return;
        selected=row;
        if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT) {
            if(selected<2) direction=x<.21f ? -1 : 1;
            else activate=true;
        }
    }
    if(selected==0 && direction) cam.lookSensitivity=glm::clamp(cam.lookSensitivity+.05f*direction,.1f,3.0f);
    if(selected==1 && direction) cam.adsSensitivity=glm::clamp(cam.adsSensitivity+.05f*direction,.1f,2.0f);
    if(selected==2 && (activate || direction)) cam.invertY=!cam.invertY;
    if(!activate) return;
    if(selected==3) input.fullscreenToggle=true;
    if(selected==4) { cam.lookSensitivity=cam.adsSensitivity=1; cam.invertY=false; }
    if(selected==5 || selected==6) { save(cam); setOpen(false); }
    if(selected==6) input.quit=true;
}

void SettingsMenu::draw(Renderer& r,const Camera& cam) const {
    if(!open) return;
    r.beginHUD();
    r.drawRect({0,0},{2,2},{0,0,0},.55f);
    r.drawRect({0,0},{1.26f,1.12f},{.055f,.07f,.08f},.97f);
    r.drawText("SETTINGS",-.54f,.43f,.045f,{.95f,.93f,.84f},1);
    const char* labels[]={"MOUSE SENSITIVITY","ADS SENSITIVITY","INVERT Y","FULLSCREEN","RESET AIM DEFAULTS","RESUME","QUIT GAME"};
    for(int i=0;i<7;++i) {
        float y=.28f-i*.105f;
        if(i==selected) r.drawRect({0,y+.014f},{1.16f,.09f},{.18f,.25f,.28f},1);
        r.drawText(labels[i],-.54f,y,.029f,{.9f,.92f,.93f},1);
        char value[32]={};
        if(i<2) snprintf(value,sizeof(value),"<  %.2fX  >",i==0 ? cam.lookSensitivity : cam.adsSensitivity);
        if(i==2) snprintf(value,sizeof(value),"%s",cam.invertY ? "ON" : "OFF");
        if(i==3) snprintf(value,sizeof(value),"%s",SDL_GetWindowFlags(r.window)&SDL_WINDOW_FULLSCREEN_DESKTOP ? "ON" : "OFF");
        r.drawText(value,.13f,y,.029f,{.92f,.78f,.45f},1);
    }
    r.drawText(saveFailed ? "COULD NOT SAVE SETTINGS" : "ARROWS / CLICK TO ADJUST - ESC TO RESUME",-.54f,-.48f,.021f,{.7f,.75f,.78f},1);
    r.drawText("MATCH CONTINUES WHILE OPEN",-.54f,-.52f,.019f,{.7f,.75f,.78f},1);
    r.endHUD();
}
