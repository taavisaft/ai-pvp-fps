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
#include "server_fire.h"
#include "server_rewind.h"
struct ClientSlot {
    bool        used = false;
    sockaddr_in addr{};
    InputState  input{};
    uint32_t    lastSeq      = 0;
    bool        hasSeq      = false;
    float       silence      = 0.0f;
    ServerFire  fire;
};

static GameState  game;
static ClientSlot clients[MAX_PLAYERS];
static bool       prevAlive[MAX_PLAYERS];

// Human-readable map name, shared by the startup log and the lobby PKT_INFO reply.
static const char* mapLabel(MapId) { return "PALDISKI"; }

// World-impact decals collected across the ticks since the last broadcast, then sent
// once per state packet and cleared. Cosmetic + unreliable; overflow just drops marks.
static Impact gImpacts[NET_MAX_IMPACTS];
static int    gImpactCount = 0;

static uint32_t fireEpochSource = 0;
// Join at the slot-selected spawn; choose a random point per respawn
// so deaths don't return you to a campable fixed spot.
static glm::vec3 spawnPos(int point) {
    glm::vec3 p = gMapSpawnCount > 0 ? gMapSpawns[point % gMapSpawnCount]
                                     : glm::vec3(0.0f);
    p.y = terrainHeight(p.x, p.z);   // sit on the ground
    return p;
}
static float spawnYaw(int point) {
    glm::vec3 p = spawnPos(point);   // face the town center-ish (map origin area)
    return glm::degrees(atan2f(-p.z, -p.x));
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
    clients[id].fire.reset(p, fireEpochSource);
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
            HelloPacket hello{};
            bool compatible = n >= (int)sizeof(hello);
            if (compatible) {
                memcpy(&hello, buf, sizeof(hello));
                compatible = hello.protocolVersion == NET_PROTOCOL_VERSION &&
                             hello.worldRevision == NET_WORLD_REVISION;
            }
            if (!compatible) {
                AcceptPacket reject{PKT_ACCEPT, 255, (uint8_t)gMapId,
                                    NET_PROTOCOL_VERSION, NET_WORLD_REVISION};
                netSend(fd, &reject, sizeof(reject), from);
                continue;
            }
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
            AcceptPacket a{PKT_ACCEPT, (uint8_t)id, (uint8_t)gMapId,
                           NET_PROTOCOL_VERSION, NET_WORLD_REVISION};
            netSend(fd, &a, sizeof(a), from);
        } else if (type == PKT_INPUT && id >= 0 && n >= (int)sizeof(InputPacket)) {
            InputPacket p;
            memcpy(&p, buf, sizeof(p));
            ClientSlot& c = clients[id];
            c.silence = 0.0f;
            if (!validFireInput(p) || (c.hasSeq && !serialNewer(p.seq, c.lastSeq))) continue;
            c.hasSeq = true;
            c.lastSeq = p.seq;
            c.input = decodeFireInput(p);
            c.fire.receive(p, game.players[id], serverTime);
        } else if (type == PKT_QUERY) {
            // Stateless lobby probe: report map + population without joining.
            int online = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) online += clients[i].used;
            InfoPacket info{};
            info.type       = PKT_INFO;
            info.mapId      = (uint8_t)gMapId;
            info.players    = (uint8_t)online;
            info.maxPlayers = (uint8_t)MAX_PLAYERS;
            snprintf(info.name, sizeof(info.name), "%s", mapLabel(gMapId));
            netSend(fd, &info, sizeof(info), from);
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
            c.fire.synchronize(p, c.input.fireMode, fireEpochSource);
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
        p.ads   = c.input.ads;                // recorded into the lag-comp snapshot
        p.pitch = c.input.pitch;              // pose the head/neck hitboxes
        p.lean  = c.input.lean;               // lean moves the upper-body hitboxes
        updateReload(p, c.input.reload, dt);
        c.fire.synchronize(p, c.input.fireMode, fireEpochSource);
        c.fire.tick(game, i, serverTime, c.input, rewindForShot);
    }

    recordSnapshot(game);                       // freshest positions for lag-comp lookups
    updateBullets(game, dt, rewindLookup, nullptr, gImpacts, &gImpactCount, NET_MAX_IMPACTS);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].used) continue;
        if (prevAlive[i] && !game.players[i].alive)
            printf("server: player %d died\n", i);
        prevAlive[i] = game.players[i].alive;
    }
}

static void broadcast(int fd, uint32_t seq) {
    static StatePacket s;  // keep off the stack, reused each call
    s.type     = PKT_STATE;
    s.seq      = seq;
    s.usedMask = game.usedMask;
    recordStateTime(seq);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Player& p = game.players[i];
        const uint8_t shots = clients[i].fire.shotsFired;  // wraps naturally for delta-on-client
        s.players[i] = {p.pos.x, p.pos.y, p.pos.z, p.yaw,
                        p.hp, (uint8_t)(p.alive ? 1 : 0),
                        (uint8_t)p.mag, (uint8_t)p.reserve, (uint8_t)(p.reloading ? 1 : 0),
                        (uint8_t)(p.kills  > 255 ? 255 : p.kills),
                        (uint8_t)(p.deaths > 255 ? 255 : p.deaths),
                        shots,
                        (uint8_t)(p.crouched ? 1 : 0),
                        p.weaponId,                          // held weapon (third-person gun)
                        clients[i].input.pitch,              // aim pitch (third-person tilt)
                        (int8_t)(clients[i].input.lean * 127.0f),  // peek lean
                        (uint8_t)(clients[i].input.ads ? 1 : 0)};  // ADS (raised-gun pose)
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
            s.fireEpoch = clients[i].fire.epoch;
            s.fireMode = clients[i].fire.mode;
            s.recvHits = (uint8_t)rp.hits;
            s.recvHitX = rp.lastHitPos.x;
            s.recvHitY = rp.lastHitPos.y;
            s.recvHitZ = rp.lastHitPos.z;
            netSend(fd, &s, size, clients[i].addr);
        }

    // World-impact decals: one batched packet to everyone, then clear the buffer.
    if (gImpactCount > 0) {
        static ImpactPacket ip;
        ip.type  = PKT_IMPACT;
        ip.count = (uint8_t)gImpactCount;
        for (int k = 0; k < gImpactCount; k++) {
            const glm::vec3& n = gImpacts[k].normal;
            uint8_t dir = n.x >  0.5f ? IMP_PX : n.x < -0.5f ? IMP_NX
                        : n.y >  0.5f ? IMP_PY : n.y < -0.5f ? IMP_NY
                        : n.z >  0.5f ? IMP_PZ : IMP_NZ;
            ip.impacts[k] = {gImpacts[k].pos.x, gImpacts[k].pos.y, gImpacts[k].pos.z, dir};
        }
        int isize = impactPacketSize(gImpactCount);
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (clients[i].used) netSend(fd, &ip, isize, clients[i].addr);
        gImpactCount = 0;
    }
}

int main() {
    setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered even when piped to a log
    srand((unsigned)time(nullptr));
    // Paldiski is the map; FPS_MAP stays as the registry hook for future maps.
    MapId mapSel = mapFromName(getenv("FPS_MAP"), MAP_PALDISKI);
    setMap(mapSel);
    printf("server: map = %s\n", mapLabel(mapSel));
    // FPS_PORT lets several map servers share one host on distinct ports; the client
    // lobby probe scans the range starting at UDP_PORT. Defaults to UDP_PORT.
    uint16_t port = UDP_PORT;
    if (const char* portEnv = getenv("FPS_PORT")) {
        int p = atoi(portEnv);
        if (p > 0 && p < 65536) port = (uint16_t)p;
    }
    platformSocketInit();
    int fd = -1;
    if (!netOpen(fd) || !netBind(fd, port)) {
        fprintf(stderr, "server: cannot bind UDP %d\n", port);
        return 1;
    }
    printf("server: listening on UDP %d (max %d players)\n", port, MAX_PLAYERS);

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
