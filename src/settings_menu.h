#pragma once
#include <SDL.h>
struct Camera;
struct Renderer;
struct FrameInput;

struct SettingsMenu {
    bool open=false;
    int selected=0;
    char path[1024]={};
    bool saveFailed=false;
    void load(Camera& cam);
    void save(const Camera& cam);
    void setOpen(bool value);
    void event(const SDL_Event& e, Camera& cam, FrameInput& input);
    void draw(Renderer& renderer, const Camera& cam) const;
};
