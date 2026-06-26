#include "lobby.h"
#include <cstdio>
#include <cstring>

// Re-broadcast the probe this often so the list stays live (new servers appear,
// player counts update) while the browser is open.
static const float REPROBE_SEC = 1.0f;

static void fireProbes(Lobby& lb) {
    QueryPacket q{PKT_QUERY};
    for (int i = 0; i < LOBBY_SCAN_PORTS; i++) {
        sockaddr_in to{};
        if (!netResolve(lb.host, (uint16_t)(UDP_PORT + i), to)) continue;
        netSend(lb.fd, &q, sizeof(q), to);
        lb.sendTime[i] = lb.clock;
    }
}

void Lobby::start(const char* ip) {
    close();
    snprintf(host, sizeof(host), "%s", ip);
    count  = 0;
    clock  = 0.0f;
    for (auto& e : entries) e = ServerEntry{};
    if (!netOpen(fd)) { fd = -1; return; }
    active = true;
    fireProbes(*this);
}

void Lobby::update(float dt) {
    if (!active || fd < 0) return;
    float prev = clock;
    clock += dt;
    // Periodic re-probe keeps populations and the server set current.
    if ((int)(clock / REPROBE_SEC) != (int)(prev / REPROBE_SEC)) fireProbes(*this);

    char buf[256];
    sockaddr_in from{};
    int n;
    while ((n = netRecv(fd, buf, sizeof(buf), from)) > 0) {
        if (n < (int)sizeof(InfoPacket) || (PacketType)buf[0] != PKT_INFO) continue;
        InfoPacket info;
        memcpy(&info, buf, sizeof(info));
        uint16_t srcPort = ntohs(from.sin_port);

        // Find existing entry for this port, or append a new one.
        int idx = -1;
        for (int i = 0; i < count; i++)
            if (entries[i].port == srcPort) { idx = i; break; }
        if (idx < 0) {
            if (count >= LOBBY_SCAN_PORTS) continue;
            idx = count++;
        }
        ServerEntry& e = entries[idx];
        e.addr       = from;
        e.port       = srcPort;
        e.mapId      = info.mapId;
        e.players    = info.players;
        e.maxPlayers = info.maxPlayers;
        snprintf(e.name, sizeof(e.name), "%.15s", info.name);
        // Ping = round-trip since this port's last probe.
        int slot = srcPort - UDP_PORT;
        if (slot >= 0 && slot < LOBBY_SCAN_PORTS)
            e.pingMs = (clock - sendTime[slot]) * 1000.0f;
    }
}

void Lobby::close() {
    if (fd >= 0) closeSocket(fd);
    fd     = -1;
    active = false;
}
