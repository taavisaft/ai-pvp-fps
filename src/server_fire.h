#pragma once
#include "game.h"
#include "protocol.h"

// Half-range serial arithmetic: duplicates, backwards and ambiguous jumps lose.
inline bool serialNewer(uint32_t value, uint32_t previous) {
    uint32_t delta = value - previous;
    return delta != 0 && delta < 0x80000000u;
}

bool validFireInput(const InputPacket& packet);
InputState decodeFireInput(const InputPacket& packet); // call after validation
using ShotRewind = float (*)(uint32_t viewSeq, uint8_t viewFrac);

// Per-client, server-owned firing state. Epochs invalidate old in-flight requests
// after weapon/mode/reload/life changes. Counters restart within each epoch.
struct ServerFire {
    static constexpr int MAX_PENDING = 4;
    static constexpr double MAX_AGE = 0.35;  // seconds after receipt, no long backlog
    uint32_t epoch = 0;
    uint8_t mode = FIRE_SEMI;
    uint8_t shotsFired = 0;                 // successful bullet spawns only

    void reset(const Player& player, uint32_t& epochSource);
    void synchronize(const Player& player, uint8_t desiredMode, uint32_t& epochSource);
    void receive(const InputPacket& packet, const Player& player, double now);
    bool tick(GameState& game, int playerID, double now, const InputState& movement = InputState{},
              ShotRewind rewind = nullptr);
    int pendingCount() const { return count; }

private:
    struct Request {
        float yaw = 0, pitch = 0, lean = 0;
        bool ads = false;
        uint32_t viewSeq = 0;
        uint8_t viewFrac = 0;
        double expires = 0;
    };
    Request pending[MAX_PENDING];
    int head = 0, count = 0;
    uint32_t lastRequest = 0;
    uint8_t weapon = WEP_UZI;
    bool alive = true, reloading = false;
    double nextAllowed = 0;
    void invalidate(uint32_t& epochSource);
};
