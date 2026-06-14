#pragma once
#include "net_common.h"
#include "protocol.h"
#include "game.h"

// Client-side connection to the dedicated server.
struct ClientNet {
    int         fd        = -1;
    sockaddr_in server{};
    int         playerID  = -1;   // assigned by ACCEPT
    bool        connecting = false;
    bool        connected  = false;
    uint32_t    inputSeq   = 0;
    uint32_t    shotSeq    = 0;   // bumped per click; carried in every input packet

    StatePacket prevState{};      // for interpolation
    StatePacket lastState{};
    bool        hasState   = false;
    bool        hasPrev    = false;
    float       sinceState = 0.0f;  // seconds since lastState arrived
    float       helloTimer = 0.0f;
    float       silence    = 0.0f;  // seconds since any server packet

    float       lagSec     = 0.0f;  // FPS_LAG one-way latency, debug (0 = off)
    float       clock      = 0.0f;  // advanced by update(dt) for delay scheduling

    bool connect(const char* ip);          // open socket, start HELLO retries
    void update(float dt);                 // drain socket, handshake, track states
    // viewSeq/viewFrac: the state seq + interpolation alpha (*255) the client is
    // rendering when this input is sent, so the server can lag-compensate shots.
    void sendInput(const InputState& in, uint32_t viewSeq, uint8_t viewFrac);
    void disconnect();                     // BYE + close

    void sendRaw(const void* buf, int len);             // delayed if FPS_LAG set
    bool processPacket(const char* buf, int n, const sockaddr_in& from);  // true => returned/disconnected
};

// Fills a render-ready GameState by interpolating between two StatePackets.
// alpha 0 = a, 1 = b. Bullets get ownerID -1 (owner not in protocol).
void unpackState(const StatePacket& a, const StatePacket& b, float alpha, GameState& out);
