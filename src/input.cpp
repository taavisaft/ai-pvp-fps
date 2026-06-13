#include "input.h"
#include "camera.h"
#include <SDL.h>

void pollInput(FrameInput& in, Camera& cam) {
    in.state.shoot      = false;
    in.wireframeToggle  = false;
    in.connectRequested = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            in.quit = true;
            break;
        case SDL_KEYDOWN:
            if (e.key.repeat) break;
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE: in.quit = true; break;
            case SDLK_f:      in.wireframeToggle = true; break;
            case SDLK_c:      in.connectRequested = true; break;
            default: break;
            }
            break;
        case SDL_MOUSEMOTION:
            cam.addLook((float)e.motion.xrel, (float)e.motion.yrel);
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT) in.state.shoot = true;
            break;
        default:
            break;
        }
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    in.state.w = keys[SDL_SCANCODE_W];
    in.state.a = keys[SDL_SCANCODE_A];
    in.state.s = keys[SDL_SCANCODE_S];
    in.state.d = keys[SDL_SCANCODE_D];
    in.state.sprint = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    in.state.jump   = keys[SDL_SCANCODE_SPACE];
    in.scoreboardHeld = keys[SDL_SCANCODE_TAB];
    in.state.yaw   = cam.yaw;
    in.state.pitch = cam.pitch;
}
