#include "input.h"
#include "camera.h"
#include "connect_prompt.h"
#include "settings_menu.h"
#include <SDL.h>
#include <cstdio>

// pollInput references these menu methods even when no settings menu is passed.
void SettingsMenu::event(const SDL_Event&, Camera&, FrameInput&) {}
void SettingsMenu::setOpen(bool) {}

static bool press(SDL_Keycode key, FrameInput& input, Camera& camera,
                  ConnectPrompt& prompt) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    if (SDL_PushEvent(&event) != 1) return false;
    pollInput(input, camera, &prompt, nullptr);
    return true;
}

int main() {
    if (SDL_Init(SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    int failures = 0;
    Camera camera;
    ConnectPrompt prompt;
    FrameInput input;
    prompt.show("127.0.0.1");
    if (!press(SDLK_RETURN, input, camera, prompt) || !input.connectSubmit)
        ++failures;
    if (input.state.w || input.state.shootHeld) ++failures;
    pollInput(input, camera, &prompt, nullptr);
    if (input.connectSubmit) ++failures; // submit remains an edge event
    if (!press(SDLK_UP, input, camera, prompt) || !input.promptUp) ++failures;
    if (!press(SDLK_DOWN, input, camera, prompt) || !input.promptDown) ++failures;
    if (!press(SDLK_KP_ENTER, input, camera, prompt) || !input.connectSubmit)
        ++failures;
    SDL_Quit();
    if (failures) std::fprintf(stderr, "connect prompt input: %d failures\n", failures);
    return failures ? 1 : 0;
}
