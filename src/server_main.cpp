// Dedicated server: drop-in FFA for up to 16 players.
// Authoritative physics @ 60 Hz, StatePacket broadcast @ 20 Hz.
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
#include <cmath>
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
    bool        pendingShoot = false;
};

static GameState  game;
static ClientSlot clients[MAX_PLAYERS];
static bool       prevAlive[MAX_PLAYERS];

// Spawn points on a circle, facing the center
static glm::vec3 spawnPos(int id) {
    float a = glm::radians(id * (360.0f / MAX_PLAYERS));
    return {15.0f * cosf(a), 0.0f, 15.0f * sinf(a)};
}
static float spawnYaw(int id) {
    return id * (360.0f / MAX_PLAYERS) + 180.0f;  // look toward center
}

static void respawn(int id) {
    Player& p = game.players[id];
    p = Player{};
    p.pos = spawnPos(id);
    p.yaw = spawnYaw(id);
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
                respawn(id);
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
            c.input.yaw   = p.yaw;
            c.input.pitch = p.pitch;
            if (p.keys & KEY_SHOOT) c.pendingShoot = true;
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
            c.pendingShoot = false;
            p.respawnTimer -= dt;
            if (p.respawnTimer <= 0.0f) {
                respawn(i);
                printf("server: player %d respawned\n", i);
            }
            continue;
        }

        movePlayer(p, c.input, dt);
        if (c.pendingShoot) {
            glm::vec3 eye = p.pos + glm::vec3(0, EYE_HEIGHT, 0);
            spawnBullet(game, eye, dirFromYawPitch(c.input.yaw, c.input.pitch), i);
            c.pendingShoot = false;
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
                        p.hp, (uint8_t)(p.alive ? 1 : 0), (uint8_t)p.ammo};
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
