#pragma once
#include "game.h"

struct Camera;

// Per-frame input snapshot + edge-triggered events
struct FrameInput {
    InputState state{};            // w/a/s/d held, shoot edge, yaw/pitch
    bool quit             = false;
    bool wireframeToggle  = false; // F pressed this frame
    bool connectRequested = false; // C pressed this frame
    bool scoreboardHeld   = false; // Tab held
    bool fireModeToggle   = false; // B pressed this frame
};

// Polls all pending SDL events. Updates camera look from mouse motion,
// fills FrameInput. shoot is true only on the frame the button was pressed.
void pollInput(FrameInput& in, Camera& cam);
