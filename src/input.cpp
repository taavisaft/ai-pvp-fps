#include "input.h"
#include "camera.h"
#include "connect_prompt.h"
#include <SDL.h>

void pollInput(FrameInput& in, Camera& cam, ConnectPrompt* connectPrompt) {
    in.state.shoot      = false;
    in.wireframeToggle  = false;
    in.connectRequested = false;
    in.fireModeToggle   = false;
    in.clearRange       = false;
    in.connectSubmit    = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (connectPrompt && connectPrompt->open) {
            switch (e.type) {
            case SDL_QUIT:
                in.quit = true;
                break;
            case SDL_TEXTINPUT:
                for (const char* t = e.text.text; *t; t++) connectPrompt->append(*t);
                break;
            case SDL_KEYDOWN:
                if (e.key.repeat) break;
                switch (e.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    in.connectSubmit = true;
                    break;
                case SDLK_ESCAPE:
                    connectPrompt->close();
                    break;
                case SDLK_BACKSPACE:
                    connectPrompt->backspace();
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
            continue;
        }
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
            case SDLK_b:      in.fireModeToggle  = true; break;
            case SDLK_g:      in.clearRange      = true; break;
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

    if (connectPrompt && connectPrompt->open) {
        in.state = InputState{};
        in.scoreboardHeld = false;
        return;
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    in.state.w = keys[SDL_SCANCODE_W];
    in.state.a = keys[SDL_SCANCODE_A];
    in.state.s = keys[SDL_SCANCODE_S];
    in.state.d = keys[SDL_SCANCODE_D];
    in.state.sprint = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    in.state.jump   = keys[SDL_SCANCODE_SPACE];
    in.state.crouch = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
    in.state.reload = keys[SDL_SCANCODE_R];
    Uint32 mouse = SDL_GetMouseState(nullptr, nullptr);
    in.state.ads       = (mouse & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    in.state.shootHeld = (mouse & SDL_BUTTON(SDL_BUTTON_LEFT))  != 0;
    in.scoreboardHeld = keys[SDL_SCANCODE_TAB];
    in.state.yaw   = cam.aimYaw();    // includes recoil offset
    in.state.pitch = cam.aimPitch();
}
