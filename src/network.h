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

    StatePacket prevState{};      // for interpolation
    StatePacket lastState{};
    bool        hasState   = false;
    bool        hasPrev    = false;
    float       sinceState = 0.0f;  // seconds since lastState arrived
    float       helloTimer = 0.0f;
    float       silence    = 0.0f;  // seconds since any server packet

    bool connect(const char* ip);          // open socket, start HELLO retries
    void update(float dt);                 // drain socket, handshake, track states
    void sendInput(const InputState& in);
    void disconnect();                     // BYE + close
};

// Fills a render-ready GameState by interpolating between two StatePackets.
// alpha 0 = a, 1 = b. Bullets get ownerID -1 (owner not in protocol).
void unpackState(const StatePacket& a, const StatePacket& b, float alpha, GameState& out);
