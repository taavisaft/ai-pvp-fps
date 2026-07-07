#pragma once
#include "game.h"

struct Camera;
struct ConnectPrompt;

// Per-frame input snapshot + edge-triggered events
struct FrameInput {
    InputState state{};            // w/a/s/d held, shoot edge, yaw/pitch
    bool quit             = false;
    bool wireframeToggle  = false; // F pressed this frame
    bool connectRequested = false; // C pressed this frame
    bool scoreboardHeld   = false; // Tab held
    bool fireModeToggle   = false; // B pressed this frame
    bool clearRange       = false; // G pressed this frame (clear offline target marks)
    bool connectSubmit    = false; // Enter pressed while connect prompt is open
    bool promptUp         = false; // Up arrow in the server browser (move selection)
    bool promptDown       = false; // Down arrow in the server browser
    int  weaponSelect     = -1;    // weapon id when 1/2 pressed this frame, else -1
    bool hitboxToggle     = false; // H pressed this frame (debug hitbox view)
    bool mapToggle        = false; // M pressed this frame (full-screen map)
    bool hudToggle        = false; // J pressed this frame (hide/show whole HUD)
    bool atmoToggle       = false; // K pressed this frame (cycle atmosphere preset)
};

// Polls all pending SDL events. Updates camera look from mouse motion,
// fills FrameInput. shoot is true only on the frame the button was pressed.
void pollInput(FrameInput& in, Camera& cam, ConnectPrompt* connectPrompt = nullptr);
