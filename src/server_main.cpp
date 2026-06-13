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

struct ClientSlot {
    bool        used = false;
    sockaddr_in addr{};
    InputState  input{};
    uint32_t    lastSeq      = 0;
    float       silence      = 0.0f;
    uint32_t    shotSeq      = 0;   // latest shot count seen from client
    uint32_t    firedShots   = 0;   // shots already spawned; fire while < shotSeq
};

static GameState  game;
static ClientSlot clients[MAX_PLAYERS];
static bool       prevAlive[MAX_PLAYERS];

// Spawn points on a circle, facing the center. Random point per respawn
// so deaths don't return you to a campable fixed spot.
static glm::vec3 spawnPos(int point) {
    float a = glm::radians(point * (360.0f / MAX_PLAYERS));
    return {15.0f * cosf(a), 0.0f, 15.0f * sinf(a)};
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
            c.input.yaw   = p.yaw;
            c.input.pitch = p.pitch;
            c.shotSeq     = p.shotSeq;  // packet is seq-gated newest, so monotonic
        } else if (type == PKT_BYE && id >= 0) {
            dropClient(id);
        }
    }
}

static void tick(float dt) {
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

        movePlayer(p, c.input, dt);
        if (c.firedShots < c.shotSeq) {   // one shot per tick; drains any backlog
            glm::vec3 eye = p.pos + glm::vec3(0, EYE_HEIGHT, 0);
            spawnBullet(game, eye, dirFromYawPitch(c.input.yaw, c.input.pitch), i);
            c.firedShots++;
        }
    }

    updateBullets(game, dt);

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

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Player& p = game.players[i];
        s.players[i] = {p.pos.x, p.pos.y, p.pos.z, p.yaw,
                        p.hp, (uint8_t)(p.alive ? 1 : 0), (uint8_t)p.ammo,
                        (uint8_t)(p.kills  > 255 ? 255 : p.kills),
                        (uint8_t)(p.deaths > 255 ? 255 : p.deaths)};
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
        if (clients[i].used) netSend(fd, &s, size, clients[i].addr);
}

int main() {
    setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered even when piped to a log
    srand((unsigned)time(nullptr));
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
