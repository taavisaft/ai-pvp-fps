#include "network.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// --- Debug FPS_LAG: client-side delay queues (single client instance) ---
namespace {
struct DPkt { char buf[1500]; int len = 0; float due = 0.0f; bool used = false; };
constexpr int QN = 256;
DPkt        outQ[QN];
DPkt        inQ[QN];
sockaddr_in inFrom[QN];

void clearQueues() {
    for (int i = 0; i < QN; i++) { outQ[i].used = false; inQ[i].used = false; }
}
}  // namespace

bool ClientNet::connect(const char* ip) {
    if (!netOpen(fd)) return false;
    if (!netResolve(ip, UDP_PORT, server)) {
        fprintf(stderr, "net: bad address %s\n", ip);
        closeSocket(fd);
        fd = -1;
        return false;
    }
    connecting = true;
    connected  = false;
    helloTimer = 1.0f;  // send first HELLO immediately
    silence    = 0.0f;
    inputSeq   = 0;     // fresh session; server slot starts clean too
    shotSeq    = 0;
    clock      = 0.0f;
    lagSec     = 0.0f;
    if (const char* e = getenv("FPS_LAG")) {
        float ms = (float)atof(e);
        if (ms > 0.0f) { lagSec = ms * 0.001f; printf("net: simulating %.0f ms one-way latency\n", ms); }
    }
    clearQueues();
    return true;
}

// Sends immediately, or enqueues for delayed delivery when FPS_LAG is set.
void ClientNet::sendRaw(const void* buf, int len) {
    if (len <= 0 || len > (int)sizeof(outQ[0].buf)) return;
    if (lagSec <= 0.0f) { netSend(fd, buf, len, server); return; }
    for (int i = 0; i < QN; i++) {
        if (outQ[i].used) continue;
        memcpy(outQ[i].buf, buf, len);
        outQ[i].len  = len;
        outQ[i].due  = clock + lagSec;
        outQ[i].used = true;
        return;
    }
    netSend(fd, buf, len, server);  // queue full: send now rather than drop
}

// Handles one received packet. Returns true if the connection was torn down
// (caller must stop touching this ClientNet).
bool ClientNet::processPacket(const char* buf, int n, const sockaddr_in& from) {
    if (!netSameAddr(from, server)) return false;
    silence = 0.0f;
    PacketType type = (PacketType)buf[0];

    if (type == PKT_ACCEPT && n >= (int)sizeof(AcceptPacket)) {
        AcceptPacket a;
        memcpy(&a, buf, sizeof(a));
        playerID   = a.playerID;
        connected  = true;
        connecting = false;
        printf("net: connected as player %d\n", playerID);
    } else if (type == PKT_STATE && n >= statePacketSize(0)) {
        if (hasState) {
            prevState = lastState;
            hasPrev   = true;
        }
        int copy = n <= (int)sizeof(StatePacket) ? n : (int)sizeof(StatePacket);
        memcpy(&lastState, buf, copy);  // truncated packet; bulletCount guards the tail
        hasState   = true;
        sinceState = 0.0f;
    } else if (type == PKT_BYE) {
        printf("net: server closed connection\n");
        disconnect();
        return true;
    }
    return false;
}

void ClientNet::update(float dt) {
    if (fd < 0) return;
    clock += dt;

    if (connecting && !connected) {
        helloTimer += dt;
        if (helloTimer >= 0.5f) {
            HelloPacket h{PKT_HELLO};
            sendRaw(&h, sizeof(h));
            helloTimer = 0.0f;
        }
    }

    if (connected) {
        silence    += dt;
        sinceState += dt;
        if (silence > 5.0f) {
            printf("net: server timed out\n");
            disconnect();
            return;
        }
    }

    // Flush outgoing packets whose simulated latency has elapsed.
    if (lagSec > 0.0f)
        for (int i = 0; i < QN; i++)
            if (outQ[i].used && clock >= outQ[i].due) {
                netSend(fd, outQ[i].buf, outQ[i].len, server);
                outQ[i].used = false;
            }

    char buf[1500];
    sockaddr_in from{};
    int n;
    while ((n = netRecv(fd, buf, sizeof(buf), from)) > 0) {
        if (lagSec <= 0.0f) {
            if (processPacket(buf, n, from)) return;
        } else {
            for (int i = 0; i < QN; i++) {       // enqueue for delayed processing
                if (inQ[i].used) continue;
                memcpy(inQ[i].buf, buf, n);
                inQ[i].len  = n;
                inQ[i].due  = clock + lagSec;
                inQ[i].used = true;
                inFrom[i]   = from;
                break;
            }
        }
    }

    if (lagSec > 0.0f)
        for (int i = 0; i < QN; i++)
            if (inQ[i].used && clock >= inQ[i].due) {
                inQ[i].used = false;
                if (processPacket(inQ[i].buf, inQ[i].len, inFrom[i])) return;
            }
}

void ClientNet::sendInput(const InputState& in, uint32_t viewSeq, uint8_t viewFrac) {
    if (!connected) return;
    InputPacket p{};
    p.type = PKT_INPUT;
    p.seq  = ++inputSeq;
    // shotSeq is bumped by the client fire logic (per fire mode), not here;
    // every packet just re-advertises the latest count for reliable delivery.
    p.keys = (in.w ? KEY_W : 0) | (in.a ? KEY_A : 0) |
             (in.s ? KEY_S : 0) | (in.d ? KEY_D : 0) |
             (in.sprint ? KEY_SPRINT : 0) | (in.jump ? KEY_JUMP : 0) |
             (in.crouch ? KEY_CROUCH : 0);
    p.yaw      = in.yaw;
    p.pitch    = in.pitch;
    p.shotSeq  = shotSeq;
    p.flags    = (in.ads ? FLAG_ADS : 0) | (in.reload ? FLAG_RELOAD : 0);
    p.viewSeq  = viewSeq;
    p.viewFrac = viewFrac;
    sendRaw(&p, sizeof(p));
}

void unpackState(const StatePacket& a, const StatePacket& b, float alpha, GameState& out) {
    out.usedMask = b.usedMask;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const PlayerNetState& pb = b.players[i];
        glm::vec3 pos{pb.x, pb.y, pb.z};
        float     yaw = pb.yaw;
        if ((a.usedMask & b.usedMask) & (1u << i)) {
            const PlayerNetState& pa = a.players[i];
            pos = glm::mix(glm::vec3{pa.x, pa.y, pa.z}, pos, alpha);
            yaw = pa.yaw + (pb.yaw - pa.yaw) * alpha;
        }
        out.players[i].pos   = pos;
        out.players[i].yaw   = yaw;
        out.players[i].hp      = pb.hp;
        out.players[i].mag     = pb.mag;
        out.players[i].reserve = pb.reserve;
        out.players[i].reloading = pb.reloading != 0;
        out.players[i].kills   = pb.kills;
        out.players[i].deaths  = pb.deaths;
        out.players[i].alive   = pb.alive != 0;
        out.players[i].crouched = pb.crouched != 0;
    }

    int count = b.bulletCount <= NET_MAX_BULLETS ? b.bulletCount : NET_MAX_BULLETS;
    int prevCount = a.bulletCount <= NET_MAX_BULLETS ? a.bulletCount : NET_MAX_BULLETS;
    for (int i = 0; i < MAX_BULLETS; i++) out.bullets[i].active = false;
    for (int i = 0; i < count; i++) {
        const BulletNetState& nb = b.bullets[i];
        glm::vec3 pos{nb.x, nb.y, nb.z};
        for (int j = 0; j < prevCount; j++) {   // match by pool slot for smooth interp
            if (a.bullets[j].poolIdx != nb.poolIdx) continue;
            pos = glm::mix(glm::vec3{a.bullets[j].x, a.bullets[j].y, a.bullets[j].z},
                           pos, alpha);
            break;
        }
        out.bullets[i].pos     = pos;
        out.bullets[i].active  = true;
        out.bullets[i].ownerID = nb.owner;
    }
    out.bulletCount = count;
}

void ClientNet::disconnect() {
    if (fd < 0) return;
    if (connected) {
        ByePacket b{PKT_BYE};
        netSend(fd, &b, sizeof(b), server);
    }
    closeSocket(fd);
    fd = -1;
    connected  = false;
    connecting = false;
    hasState   = false;
    hasPrev    = false;
    playerID   = -1;
}
