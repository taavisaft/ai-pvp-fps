// Dedicated server: drop-in FFA for up to 16 players.
// Authoritative physics @ 60 Hz, StatePacket broadcast @ 20 Hz.
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include "platform.h"
#include "net_common.h"
#include "protocol.h"
#include "game.h"
#include "physics.h"
#include "map.h"

struct ClientSlot {
    bool        used = false;
    sockaddr_in addr{};
    InputState  input{};
    uint32_t    lastSeq      = 0;
    float       silence      = 0.0f;
    uint32_t    shotSeq      = 0;   // latest shot count seen from client
    uint32_t    firedShots   = 0;   // shots already spawned; fire while < shotSeq
    uint32_t    viewSeq      = 0;   // state the client was rendering (lag comp)
    uint8_t     viewFrac     = 0;   // interpolation alpha * 255 (lag comp)
};

static GameState  game;
static ClientSlot clients[MAX_PLAYERS];
static bool       prevAlive[MAX_PLAYERS];

// --- Lag compensation: rewind player hitboxes to the shooter's view-time ---
constexpr int   HISTORY_TICKS   = 90;    // 1.5 s of position history at 60 Hz
constexpr float HISTORY_SECONDS = 1.4f;  // max rewind, kept under the buffer
constexpr int   STATE_HIST      = 64;    // recent state-seq -> send-time map size

static float serverTime = 0.0f;          // monotonic seconds since boot

// One per-tick snapshot of every player's hitbox, tagged with its server time.
struct Snap {
    float     t = 0.0f;
    uint16_t  usedMask = 0;
    glm::vec3 pos[MAX_PLAYERS];
    bool      crouched[MAX_PLAYERS];
    bool      alive[MAX_PLAYERS];
};
static Snap history[HISTORY_TICKS];
static int  histHead  = -1;   // index of newest snapshot
static int  histCount = 0;    // valid snapshots in the ring

// Maps a broadcast state seq to the server time it was sent, so an incoming
// viewSeq can be turned into an absolute server time.
struct StateStamp { uint32_t seq = 0xFFFFFFFFu; float t = 0.0f; };
static StateStamp stateStamps[STATE_HIST];

static bool stateTimeFor(uint32_t seq, float& outT) {
    const StateStamp& s = stateStamps[seq % STATE_HIST];
    if (s.seq != seq) return false;   // unknown / overwritten
    outT = s.t;
    return true;
}

static void recordSnapshot() {
    histHead = (histHead + 1) % HISTORY_TICKS;
    Snap& s = history[histHead];
    s.t        = serverTime;
    s.usedMask = game.usedMask;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        s.pos[i]      = game.players[i].pos;
        s.crouched[i] = game.players[i].crouched;
        s.alive[i]    = game.players[i].alive;
    }
    if (histCount < HISTORY_TICKS) histCount++;
}

// RewindLookup: where player `pid`'s hitbox was `rewindSec` seconds ago.
static bool rewindLookup(const void* ctx, int pid, float rewindSec,
                         glm::vec3& pos, bool& crouched, bool& alive) {
    (void)ctx;
    if (histCount == 0) return false;
    float    target = serverTime - rewindSec;
    uint16_t bit    = (uint16_t)(1u << pid);

    int   olderIdx = histHead, newerIdx = histHead;
    float a = 0.0f;
    const Snap& newest = history[histHead];
    if (target >= newest.t || histCount == 1) {
        olderIdx = newerIdx = histHead;          // at/after newest: no interp
    } else {
        bool found = false;
        for (int k = 1; k < histCount; k++) {
            int oi = (histHead - k + HISTORY_TICKS) % HISTORY_TICKS;
            if (history[oi].t <= target) {
                olderIdx = oi;
                newerIdx = (histHead - (k - 1) + HISTORY_TICKS) % HISTORY_TICKS;
                float span = history[newerIdx].t - history[oi].t;
                a = span > 1e-6f ? (target - history[oi].t) / span : 0.0f;
                found = true;
                break;
            }
        }
        if (!found)                              // older than everything retained
            olderIdx = newerIdx = (histHead - (histCount - 1) + HISTORY_TICKS) % HISTORY_TICKS;
    }

    const Snap& sa = history[olderIdx];
    const Snap& sb = history[newerIdx];
    if (!(sb.usedMask & bit) || !sb.alive[pid]) return false;  // not a valid target then
    glm::vec3 older = ((sa.usedMask & bit) && sa.alive[pid]) ? sa.pos[pid] : sb.pos[pid];
    pos      = glm::mix(older, sb.pos[pid], a);
    crouched = sb.crouched[pid];
    alive    = true;
    return true;
}

// Spawn points on a circle, facing the center. Random point per respawn
// so deaths don't return you to a campable fixed spot.
static glm::vec3 spawnPos(int point) {
    float a = glm::radians(point * (360.0f / MAX_PLAYERS));
    // radius 12 keeps every point clear of the warehouse perimeter walls (+/-15 Z)
    return {12.0f * cosf(a), 0.0f, 12.0f * sinf(a)};
}
static float spawnYaw(int point) {
    return point * (360.0f / MAX_PLAYERS) + 180.0f;  // look toward center
}

// joining=true: fresh player at the slot's own point, score zeroed.
// joining=false: death respawn at a random point, score kept.
static void respawn(int id, bool joining) {
    Player& p = game.players[id];
    int point  = joining ? id : rand() % MAX_PLAYERS;
    int kills  = joining ? 0 : p.kills;
    int deaths = joining ? 0 : p.deaths;
    p = Player{};
    p.pos    = spawnPos(point);
    p.yaw    = spawnYaw(point);
    p.kills  = kills;
    p.deaths = deaths;
    prevAlive[id] = true;
}

static void dropClient(int id) {
    clients[id] = ClientSlot{};
    game.usedMask &= ~(1u << id);
    int online = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) online += clients[i].used;
    printf("server: player %d left (%d online)\n", id, online);
}

static void handlePackets(int fd) {
    char buf[1500];
    sockaddr_in from{};
    int n;
    while ((n = netRecv(fd, buf, sizeof(buf), from)) > 0) {
        PacketType type = (PacketType)buf[0];

        int id = -1;
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (clients[i].used && netSameAddr(clients[i].addr, from)) id = i;

        if (type == PKT_HELLO) {
            if (id < 0) {
                for (int i = 0; i < MAX_PLAYERS && id < 0; i++)
                    if (!clients[i].used) id = i;
                if (id < 0) continue;  // full — reject silently
                clients[id].used = true;
                clients[id].addr = from;
                clients[id].silence = 0.0f;
                game.usedMask |= (1u << id);
                respawn(id, true);  // joining: your slot's point, fresh score
                int online = 0;
                for (int i = 0; i < MAX_PLAYERS; i++) online += clients[i].used;
                printf("server: player %d joined (%d online)\n", id, online);
            }
            AcceptPacket a{PKT_ACCEPT, (uint8_t)id};
            netSend(fd, &a, sizeof(a), from);
        } else if (type == PKT_INPUT && id >= 0 && n >= (int)sizeof(InputPacket)) {
            InputPacket p;
            memcpy(&p, buf, sizeof(p));
            ClientSlot& c = clients[id];
            c.silence = 0.0f;
            if (p.seq <= c.lastSeq) continue;  // stale/duplicate
            c.lastSeq = p.seq;
            c.input.w = p.keys & KEY_W;
            c.input.a = p.keys & KEY_A;
            c.input.s = p.keys & KEY_S;
            c.input.d = p.keys & KEY_D;
            c.input.sprint = p.keys & KEY_SPRINT;
            c.input.jump   = p.keys & KEY_JUMP;
            c.input.crouch = p.keys & KEY_CROUCH;
            c.input.ads    = p.flags & FLAG_ADS;
            c.input.reload = p.flags & FLAG_RELOAD;
            c.input.weaponId = p.weaponId;
            c.input.yaw   = p.yaw;
            c.input.pitch = p.pitch;
            c.shotSeq     = p.shotSeq;  // packet is seq-gated newest, so monotonic
            c.viewSeq     = p.viewSeq;  // for lag-compensating this client's shots
            c.viewFrac    = p.viewFrac;
        } else if (type == PKT_BYE && id >= 0) {
            dropClient(id);
        }
    }
}

static void tick(float dt) {
    serverTime += dt;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        ClientSlot& c = clients[i];
        if (!c.used) continue;
        Player& p = game.players[i];

        if (!p.alive) {
            c.firedShots = c.shotSeq;   // drop shots queued while dead
            p.respawnTimer -= dt;
            if (p.respawnTimer <= 0.0f) {
                respawn(i, false);
                printf("server: player %d respawned\n", i);
            }
            continue;
        }

        if (p.weaponId != c.input.weaponId)   // client swapped weapons (1/2 keys)
            giveWeapon(p, c.input.weaponId);
        movePlayer(p, c.input, dt);
        updateReload(p, c.input.reload, dt);
        if (c.firedShots < c.shotSeq) {   // one shot per tick; drains any backlog
            float eyeH = p.crouched ? CROUCH_EYE : EYE_HEIGHT;
            glm::vec3 eye = p.pos + glm::vec3(0, eyeH, 0);
            glm::vec3 origin, dir;
            float spread = aimSpread(p, c.input);   // stance/movement accuracy
            weaponShot(c.input.yaw, c.input.pitch, eye, c.input.ads, spread, origin, dir);
            // Rewind targets to the world the shooter saw when they fired.
            float viewT, comp = 0.0f;
            if (stateTimeFor(c.viewSeq, viewT)) {
                comp = serverTime - (viewT + (c.viewFrac / 255.0f) * (1.0f / NET_HZ));
                if (comp < 0.0f) comp = 0.0f;
                if (comp > HISTORY_SECONDS) comp = HISTORY_SECONDS;
            }
            spawnBullet(game, origin, dir, i, comp);   // no-op if mag empty / reloading
            c.firedShots++;
        }
    }

    recordSnapshot();                       // freshest positions for lag-comp lookups
    updateBullets(game, dt, rewindLookup, nullptr);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].used) continue;
        if (prevAlive[i] && !game.players[i].alive)
            printf("server: player %d died\n", i);
        prevAlive[i] = game.players[i].alive;
    }
}

static void broadcast(int fd, uint32_t seq) {
    static StatePacket s;  // ~1.3 KB; keep off the stack, reused each call
    s.type     = PKT_STATE;
    s.seq      = seq;
    s.usedMask = game.usedMask;
    stateStamps[seq % STATE_HIST] = {seq, serverTime};  // for lag-comp viewSeq mapping

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Player& p = game.players[i];
        const uint8_t shots = (uint8_t)clients[i].firedShots;  // wraps naturally for delta-on-client
        s.players[i] = {p.pos.x, p.pos.y, p.pos.z, p.yaw,
                        p.hp, (uint8_t)(p.alive ? 1 : 0),
                        (uint8_t)p.mag, (uint8_t)p.reserve, (uint8_t)(p.reloading ? 1 : 0),
                        (uint8_t)(p.kills  > 255 ? 255 : p.kills),
                        (uint8_t)(p.deaths > 255 ? 255 : p.deaths),
                        shots,
                        (uint8_t)(p.crouched ? 1 : 0)};
    }

    uint8_t count = 0;
    for (int i = 0; i < MAX_BULLETS && count < NET_MAX_BULLETS; i++) {
        const Bullet& b = game.bullets[i];
        if (!b.active) continue;
        s.bullets[count] = {(uint8_t)i, (uint8_t)b.ownerID, b.pos.x, b.pos.y, b.pos.z};
        count++;
    }
    s.bulletCount = count;

    int size = statePacketSize(count);
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (clients[i].used) {
            const Player& rp = game.players[i];     // stamp this recipient's own hit info
            s.recvHits = (uint8_t)rp.hits;
            s.recvHitX = rp.lastHitPos.x;
            s.recvHitY = rp.lastHitPos.y;
            s.recvHitZ = rp.lastHitPos.z;
            netSend(fd, &s, size, clients[i].addr);
        }
}

int main() {
    setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered even when piped to a log
    srand((unsigned)time(nullptr));
    setMap(MAP_WAREHOUSE);                 // matches run the warehouse map
    platformSocketInit();
    int fd = -1;
    if (!netOpen(fd) || !netBind(fd, UDP_PORT)) {
        fprintf(stderr, "server: cannot bind UDP %d\n", UDP_PORT);
        return 1;
    }
    printf("server: listening on UDP %d (max %d players)\n", UDP_PORT, MAX_PLAYERS);

    const float DT = 1.0f / PHYS_HZ;
    const int   TICKS_PER_STATE = PHYS_HZ / NET_HZ;  // 3
    uint32_t    stateSeq  = 0;
    int         tickCount = 0;

    auto next = std::chrono::steady_clock::now();
    while (true) {
        handlePackets(fd);
        tick(DT);

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!clients[i].used) continue;
            clients[i].silence += DT;
            if (clients[i].silence > 5.0f) dropClient(i);
        }

        if (++tickCount >= TICKS_PER_STATE) {
            tickCount = 0;
            broadcast(fd, ++stateSeq);
        }

        next += std::chrono::microseconds(1000000 / PHYS_HZ);
        std::this_thread::sleep_until(next);
    }

    closeSocket(fd);
    platformSocketCleanup();
    return 0;
}
