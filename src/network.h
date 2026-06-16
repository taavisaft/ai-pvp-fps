#pragma once
#include "net_common.h"
#include "protocol.h"
#include "game.h"

// Client-side connection to the dedicated server.
struct ClientNet {
    int         fd        = -1;
    sockaddr_in server{};
    int         playerID  = -1;   // assigned by ACCEPT
    int         serverMap = -1;   // MapId the server runs, learned from ACCEPT
    bool        connecting = false;
    bool        connected  = false;
    uint32_t    inputSeq   = 0;
    uint32_t    shotSeq    = 0;   // bumped per click; carried in every input packet

    // Playout buffer: a ring of recent snapshots keyed by seq lets rendering run a
    // fixed delay behind the newest packet, absorbing jitter and bridging dropped
    // packets instead of freezing-then-snapping to the latest one.
    static constexpr int SNAP_HIST = 32;
    StatePacket snaps[SNAP_HIST]{};         // indexed by seq % SNAP_HIST
    bool        snapUsed[SNAP_HIST] = {false};
    uint32_t    newestSeq  = 0;             // highest seq received so far
    float       playSeq    = 0.0f;          // render position on the server seq timeline
    bool        playInit   = false;

    StatePacket lastState{};      // newest authoritative snapshot (HP/ammo/hits reads)
    bool        hasState   = false;
    float       helloTimer = 0.0f;
    float       silence    = 0.0f;  // seconds since any server packet

    float       lagSec     = 0.0f;  // FPS_LAG    one-way latency (s),  debug (0 = off)
    float       jitterSec  = 0.0f;  // FPS_JITTER +/- latency (s),      debug
    float       lossFrac   = 0.0f;  // FPS_LOSS   packet drop chance,   debug (0..1)
    float       clock      = 0.0f;  // advanced by update(dt) for delay scheduling

    bool connect(const char* ip);          // open socket, start HELLO retries
    void update(float dt);                 // drain socket, handshake, track states
    // viewSeq/viewFrac: the playout seq + fraction (*255) the client is rendering
    // when this input is sent, so the server can lag-compensate shots.
    void sendInput(const InputState& in, uint32_t viewSeq, uint8_t viewFrac);
    void disconnect();                     // BYE + close

    void sendRaw(const void* buf, int len);             // delayed/dropped if sim on
    bool processPacket(const char* buf, int n, const sockaddr_in& from);  // true => returned/disconnected

    bool simEnabled() const { return lagSec > 0.0f || jitterSec > 0.0f || lossFrac > 0.0f; }
    bool hasSnap(uint32_t s) const {
        return snapUsed[s % SNAP_HIST] && snaps[s % SNAP_HIST].seq == s;
    }
    // Picks the two snapshots bracketing the current playout position and the blend
    // factor between them; false only before the first state. Bridges a dropped
    // snapshot by interpolating across the seq gap to the next available one.
    bool sampleRender(StatePacket& a, StatePacket& b, float& alpha) const;
};

// Fills a render-ready GameState by interpolating between two StatePackets.
// alpha 0 = a, 1 = b. Bullets get ownerID -1 (owner not in protocol).
void unpackState(const StatePacket& a, const StatePacket& b, float alpha, GameState& out);
