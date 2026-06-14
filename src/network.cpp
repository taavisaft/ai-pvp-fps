#include "network.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

// Playout tuning: render this many snapshots behind the newest (at NET_HZ, 2 =
// ~100 ms) so jitter and a single dropped packet don't starve interpolation.
static const float INTERP_DELAY = 2.0f;
static const float RESYNC_RATE  = 2.0f;   // how fast the playout clock eases to target

// --- Debug network sim (FPS_LAG/JITTER/LOSS): client-side delay queues ---
namespace {
struct DPkt { char buf[1500]; int len = 0; float due = 0.0f; bool used = false; };
constexpr int QN = 256;
DPkt        outQ[QN];
DPkt        inQ[QN];
sockaddr_in inFrom[QN];

void  clearQueues() { for (int i = 0; i < QN; i++) { outQ[i].used = false; inQ[i].used = false; } }
float frand01()     { return (float)rand() / (float)RAND_MAX; }
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
    jitterSec  = 0.0f;
    lossFrac   = 0.0f;
    newestSeq  = 0;
    playSeq    = 0.0f;
    playInit   = false;
    for (int i = 0; i < SNAP_HIST; i++) snapUsed[i] = false;
    if (const char* e = getenv("FPS_LAG")) {
        float ms = (float)atof(e);
        if (ms > 0.0f) { lagSec = ms * 0.001f; printf("net: simulating %.0f ms one-way latency\n", ms); }
    }
    if (const char* e = getenv("FPS_JITTER")) {
        float ms = (float)atof(e);
        if (ms > 0.0f) { jitterSec = ms * 0.001f; printf("net: simulating +/-%.0f ms jitter\n", ms); }
    }
    if (const char* e = getenv("FPS_LOSS")) {
        float pct = (float)atof(e);
        if (pct > 0.0f) {
            lossFrac = pct * 0.01f;
            if (lossFrac > 0.95f) lossFrac = 0.95f;
            printf("net: simulating %.0f%% packet loss\n", lossFrac * 100.0f);
        }
    }
    clearQueues();
    return true;
}

// Sends immediately, or (when the debug sim is on) drops/delays the packet to
// emulate latency, jitter and loss on the uplink.
void ClientNet::sendRaw(const void* buf, int len) {
    if (len <= 0 || len > (int)sizeof(outQ[0].buf)) return;
    if (!simEnabled()) { netSend(fd, buf, len, server); return; }
    if (lossFrac > 0.0f && frand01() < lossFrac) return;   // dropped uplink packet
    float due = clock + lagSec;
    if (jitterSec > 0.0f) { due += (frand01() * 2.0f - 1.0f) * jitterSec; if (due < clock) due = clock; }
    for (int i = 0; i < QN; i++) {
        if (outQ[i].used) continue;
        memcpy(outQ[i].buf, buf, len);
        outQ[i].len  = len;
        outQ[i].due  = due;
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
        StatePacket s{};
        int copy = n <= (int)sizeof(StatePacket) ? n : (int)sizeof(StatePacket);
        memcpy(&s, buf, copy);  // truncated packet; bulletCount guards the tail
        uint32_t seq = s.seq;
        // Drop only snapshots too old to fit the ring; dups/out-of-order within the
        // window just refill their slot (the seq key keeps lookups correct).
        bool tooOld = hasState && seq <= newestSeq && (newestSeq - seq) >= (uint32_t)SNAP_HIST;
        if (!tooOld) {
            snaps[seq % SNAP_HIST]    = s;
            snapUsed[seq % SNAP_HIST] = true;
            if (!hasState || seq > newestSeq)      newestSeq = seq;
            if (!hasState || seq >= lastState.seq) lastState = s;  // newest authoritative
            hasState = true;
        }
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
        silence += dt;
        if (silence > 5.0f) {
            printf("net: server timed out\n");
            disconnect();
            return;
        }
    }

    bool sim = simEnabled();

    // Flush outgoing packets whose simulated latency has elapsed.
    if (sim)
        for (int i = 0; i < QN; i++)
            if (outQ[i].used && clock >= outQ[i].due) {
                netSend(fd, outQ[i].buf, outQ[i].len, server);
                outQ[i].used = false;
            }

    char buf[1500];
    sockaddr_in from{};
    int n;
    while ((n = netRecv(fd, buf, sizeof(buf), from)) > 0) {
        if (!sim) { if (processPacket(buf, n, from)) return; continue; }
        if (lossFrac > 0.0f && frand01() < lossFrac) continue;   // dropped downlink packet
        for (int i = 0; i < QN; i++) {       // enqueue for delayed/jittered processing
            if (inQ[i].used) continue;
            memcpy(inQ[i].buf, buf, n);
            inQ[i].len = n;
            float due = clock + lagSec;
            if (jitterSec > 0.0f) { due += (frand01() * 2.0f - 1.0f) * jitterSec; if (due < clock) due = clock; }
            inQ[i].due  = due;
            inQ[i].used = true;
            inFrom[i]   = from;
            break;
        }
    }

    if (sim)
        for (int i = 0; i < QN; i++)
            if (inQ[i].used && clock >= inQ[i].due) {
                inQ[i].used = false;
                if (processPacket(inQ[i].buf, inQ[i].len, inFrom[i])) return;
            }

    // Advance the playout clock to sit INTERP_DELAY snapshots behind the newest,
    // easing toward that target so jitter/late packets don't jolt the render.
    if (connected && hasState) {
        float target = (float)newestSeq - INTERP_DELAY;
        if (!playInit) { playSeq = target; playInit = true; }
        else {
            playSeq += dt * NET_HZ;
            playSeq += (target - playSeq) * fminf(1.0f, dt * RESYNC_RATE);
        }
        float lo = (float)newestSeq - (float)(SNAP_HIST - 1);
        if (lo < 0.0f) lo = 0.0f;
        if (playSeq < lo)                 playSeq = lo;
        if (playSeq > (float)newestSeq)   playSeq = (float)newestSeq;  // never extrapolate ahead
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

bool ClientNet::sampleRender(StatePacket& a, StatePacket& b, float& alpha) const {
    if (!hasState) return false;
    uint32_t base = (uint32_t)floorf(playSeq);

    // newest snapshot at or before the playout position
    int aseq = -1;
    for (int k = 0; k < SNAP_HIST; k++) {
        if ((uint32_t)k > base) break;          // no snapshots below seq 0
        uint32_t s = base - (uint32_t)k;
        if (hasSnap(s)) { aseq = (int)s; break; }
    }
    if (aseq < 0) { a = b = lastState; alpha = 0.0f; return true; }  // gap below window
    a = snaps[(uint32_t)aseq % SNAP_HIST];

    // next available snapshot after a (bridges a dropped one), up to the newest
    uint32_t bseq = (uint32_t)aseq;
    for (uint32_t s = (uint32_t)aseq + 1; s <= newestSeq; s++)
        if (hasSnap(s)) { bseq = s; break; }
    if (bseq == (uint32_t)aseq) { b = a; alpha = 0.0f; return true; }  // at the head: hold

    b = snaps[bseq % SNAP_HIST];
    alpha = (playSeq - (float)aseq) / (float)(bseq - (uint32_t)aseq);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    return true;
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
    playInit   = false;
    playerID   = -1;
}
