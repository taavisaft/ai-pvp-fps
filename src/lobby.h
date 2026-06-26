#pragma once
#include "net_common.h"
#include "protocol.h"
#include "game.h"

// Client-side lobby browser: probes a host's port range with PKT_QUERY and collects
// PKT_INFO replies, so the connection screen can list every map server running on
// that box. Stateless and short-lived — open one socket, fan out queries, drain
// answers for ~1 s. Joining still goes through ClientNet::connect(ip, port).
struct ServerEntry {
    sockaddr_in addr{};                 // resolved host:port to connect to
    uint16_t    port       = 0;
    int         mapId      = -1;
    int         players    = 0;
    int         maxPlayers = 0;
    char        name[16]   = {};
    float       pingMs     = -1.0f;     // round-trip of the probe, -1 until answered
};

struct Lobby {
    int         fd     = -1;
    bool        active = false;         // true while a scan socket is open
    float       clock  = 0.0f;          // seconds since start(), for ping + retries
    char        host[64] = {};
    float       sendTime[LOBBY_SCAN_PORTS] = {};  // when each port was probed

    ServerEntry entries[LOBBY_SCAN_PORTS];
    int         count = 0;

    void start(const char* ip);         // open socket, fire PKT_QUERY at the range
    void update(float dt);              // drain replies, refresh ping; re-probe slowly
    void close();                       // shut the scan socket
};
