#pragma once
#include "renderer.h"
#include "game.h"
#include "protocol.h"

struct ConnectPrompt;

// Rolling kill-feed derived from kills/deaths counter deltas between
// consecutive StatePackets (the protocol carries no explicit kill events).
struct KillFeed {
    struct Entry { char text[32]; float ttl; };
    Entry entries[4];
    int   count = 0;

    void push(int killer, int victim);
    void update(float dt);
};

struct HudState {
    float    flashTimer = 0.0f;   // red hit flash
    float    deathTimer = 0.0f;   // local respawn countdown while dead
    float    fps        = 0.0f;   // smoothed frames/sec, top-left readout
    int      fireMode   = 0;      // FireMode enum, for the HUD label
    KillFeed feed;

    float     hitMarkerTimer    = 0.0f;        // >0 while showing the hit X
    glm::vec2 hitMarkerNDC       = {0, 0};      // screen position (set in main.cpp)
    bool      hitMarkerOnScreen = false;        // impact point is in front of camera
    float     adsT              = 0.0f;         // ADS blend (red-dot reticle on Uzi)
    uint8_t  prevKills[MAX_PLAYERS]  = {0};
    uint8_t  prevDeaths[MAX_PLAYERS] = {0};
    bool     tracked = false;

    // Diff score counters of a fresh authoritative state into feed entries
    void noteState(const StatePacket& s);
};

void drawHUD(Renderer& r, const GameState& gs, int localID,
             const HudState& hud, bool scoreboard, bool online, bool fullMap);
void drawConnectPrompt(Renderer& r, const ConnectPrompt& prompt);
