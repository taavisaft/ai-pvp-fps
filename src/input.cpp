#include "input.h"
#include "camera.h"
#include "settings_menu.h"
#include "connect_prompt.h"
#include <SDL.h>

void pollInput(FrameInput& in, Camera& cam, ConnectPrompt* connectPrompt, SettingsMenu* settings) {
    bool menuCaptured = settings && settings->open;
    static bool suppressMouseUntilRelease=false;
    in.state.shoot      = false;
    in.wireframeToggle  = false;
    in.fullscreenToggle = false;
    in.connectRequested = false;
    in.fireModeToggle   = false;
    in.clearRange       = false;
    in.connectSubmit    = false;
    in.promptUp         = false;
    in.promptDown       = false;
    in.weaponSelect     = -1;
    in.hitboxToggle     = false;
    in.runnersToggle    = false;
    in.mapToggle        = false;
    in.hudToggle        = false;
    in.atmoToggle       = false;
    in.thirdPersonToggle = false;

    float wheelAccum = 0.0f;   // summed scroll delta this frame (y, or x when Shift maps it)

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN && !e.key.repeat &&
            (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) &&
            (e.key.keysym.mod & KMOD_ALT)) {
            in.fullscreenToggle = true;
            continue;
        }
        if(settings && settings->open) {
            settings->event(e,cam,in);
            menuCaptured=true;
            continue;
        }
        if(menuCaptured) { if(e.type==SDL_QUIT) in.quit=true; continue; }
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
                case SDLK_UP:
                    in.promptUp = true;
                    break;
                case SDLK_DOWN:
                    in.promptDown = true;
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
            case SDLK_ESCAPE:
                if(settings) { settings->setOpen(true); menuCaptured=true; }
                else in.quit=true;
                break;
            case SDLK_f:      in.wireframeToggle = true; break;
            case SDLK_c:      in.connectRequested = true; break;
            case SDLK_b:      in.fireModeToggle  = true; break;
            case SDLK_g:      in.clearRange      = true; break;
            case SDLK_1:      in.weaponSelect    = WEP_UZI;     break;
            case SDLK_2:      in.weaponSelect    = WEP_GLOCK19; break;
            case SDLK_3:      in.weaponSelect    = WEP_KAR98;   break;
            case SDLK_h:      in.hitboxToggle    = true;        break;
            case SDLK_F6:     in.runnersToggle   = true;        break;
            case SDLK_m:      in.mapToggle       = true;        break;
            case SDLK_j:      in.hudToggle       = true;        break;
            case SDLK_k:      in.atmoToggle      = true;        break;
            case SDLK_v:      in.thirdPersonToggle = true;      break;
            default: break;
            }
            break;
        case SDL_MOUSEMOTION:
            cam.addLook((float)e.motion.xrel, (float)e.motion.yrel);
            break;
        case SDL_MOUSEWHEEL: {
            // Accumulate; cycle once per gesture below. macOS maps scroll to the X
            // axis while Shift is held (sprint), so fall back to x when y is zero.
            float d = e.wheel.y != 0 ? (float)e.wheel.y : (float)e.wheel.x;
            if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) d = -d;
            wheelAccum += d;
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT) in.state.shoot = true;
            break;
        default:
            break;
        }
    }

    // Cycle weapon once when a scroll gesture starts. A trackpad/Magic-Mouse flick
    // fires many wheel events with momentum. Rising edge = one step/flick.
    static bool wheelWasActive = false;
    bool wheelActive = wheelAccum != 0.0f;
    if (wheelActive && !wheelWasActive && in.weaponSelect < 0) {
        int dir = wheelAccum > 0.0f ? 1 : -1;
        in.weaponSelect = ((int)gWeaponId + dir + WEP_COUNT) % WEP_COUNT;
    }
    wheelWasActive = wheelActive;

    if (menuCaptured || (connectPrompt && connectPrompt->open)) {
        bool quit=in.quit, fullscreen=in.fullscreenToggle;
        in=FrameInput{}; in.quit=quit; in.fullscreenToggle=fullscreen;
        suppressMouseUntilRelease=true;
        in.state = InputState{};
        in.state.yaw=cam.aimYaw();
        in.state.pitch=cam.aimPitch();
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
    in.state.leanLeft  = keys[SDL_SCANCODE_Q];
    in.state.leanRight = keys[SDL_SCANCODE_E];
    Uint32 mouse = SDL_GetMouseState(nullptr, nullptr);
    if(suppressMouseUntilRelease) {
        suppressMouseUntilRelease=(mouse & (SDL_BUTTON(SDL_BUTTON_LEFT)|SDL_BUTTON(SDL_BUTTON_RIGHT)))!=0;
        mouse=0; in.state.shoot=false;
    }
    in.state.ads       = (mouse & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    in.state.shootHeld = (mouse & SDL_BUTTON(SDL_BUTTON_LEFT))  != 0;
    in.scoreboardHeld = keys[SDL_SCANCODE_TAB];
    in.state.yaw   = cam.aimYaw();    // includes recoil offset
    in.state.pitch = cam.aimPitch();
}
