// Dedicated server: authoritative physics @ 60 Hz, StatePacket broadcast @ 20 Hz
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
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
static ClientSlot clients[2];
static int        prevHP[2] = {PLAYER_HP, PLAYER_HP};

static const glm::vec3 SPAWN[2] = {{0, 0, 5}, {0, 0, -5}};
static const float SPAWN_YAW[2] = {-90.0f, 90.0f};  // face each other... (yaw -90 = -Z)

static void resetMatch() {
    game = GameState{};
    for (int i = 0; i < 2; i++) {
        game.players[i].pos = SPAWN[i];
        game.players[i].yaw = SPAWN_YAW[i];
        prevHP[i] = PLAYER_HP;
    }
    printf("server: match reset\n");
}

static void dropClient(int id) {
    printf("server: player %d left\n", id);
    clients[id] = ClientSlot{};
    resetMatch();
}

static void handlePackets(int fd) {
    char buf[1024];
    sockaddr_in from{};
    int n;
    while ((n = netRecv(fd, buf, sizeof(buf), from)) > 0) {
        PacketType type = (PacketType)buf[0];

        int id = -1;
        for (int i = 0; i < 2; i++)
            if (clients[i].used && netSameAddr(clients[i].addr, from)) id = i;

        if (type == PKT_HELLO) {
            if (id < 0) {
                for (int i = 0; i < 2 && id < 0; i++)
                    if (!clients[i].used) id = i;
                if (id < 0) continue;  // full — reject silently
                clients[id].used = true;
                clients[id].addr = from;
                clients[id].silence = 0.0f;
                game.players[id] = Player{};
                game.players[id].pos = SPAWN[id];
                game.players[id].yaw = SPAWN_YAW[id];
                printf("server: player %d joined\n", id);
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
    if (game.gameOver) return;
    for (int i = 0; i < 2; i++) {
        ClientSlot& c = clients[i];
        if (!c.used) continue;
        Player& p = game.players[i];
        if (!p.alive) continue;

        movePlayer(p, c.input, dt);
        if (c.pendingShoot) {
            glm::vec3 eye = p.pos + glm::vec3(0, EYE_HEIGHT, 0);
            spawnBullet(game, eye, dirFromYawPitch(c.input.yaw, c.input.pitch), i);
            c.pendingShoot = false;
        }
    }
    updateBullets(game, dt);

    for (int i = 0; i < 2; i++) {
        if (game.players[i].hp != prevHP[i]) {
            prevHP[i] = game.players[i].hp;
            printf("server: HP p0 %d | p1 %d\n", game.players[0].hp, game.players[1].hp);
        }
    }
    if (game.gameOver) printf("server: player %d wins\n", game.winnerID);
}

static void broadcast(int fd, uint32_t seq) {
    StatePacket s{};
    s.type = PKT_STATE;
    s.seq  = seq;
    s.p0x = game.players[0].pos.x; s.p0y = game.players[0].pos.y;
    s.p0z = game.players[0].pos.z; s.p0yaw = game.players[0].yaw;
    s.p0hp = game.players[0].hp;
    s.p1x = game.players[1].pos.x; s.p1y = game.players[1].pos.y;
    s.p1z = game.players[1].pos.z; s.p1yaw = game.players[1].yaw;
    s.p1hp = game.players[1].hp;

    uint8_t count = 0;
    for (int i = 0; i < MAX_BULLETS && count < 16; i++) {
        const Bullet& b = game.bullets[i];
        if (!b.active) continue;
        s.bullets[count] = {b.pos.x, b.pos.y, b.pos.z};
        count++;
    }
    s.bulletCount = count;
    s.gameOver = game.gameOver ? 1 : 0;
    s.winnerID = (int8_t)game.winnerID;

    for (int i = 0; i < 2; i++)
        if (clients[i].used) netSend(fd, &s, sizeof(s), clients[i].addr);
}

int main() {
    setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered even when piped to a log
    platformSocketInit();
    int fd = -1;
    if (!netOpen(fd) || !netBind(fd, UDP_PORT)) {
        fprintf(stderr, "server: cannot bind UDP %d\n", UDP_PORT);
        return 1;
    }
    printf("server: listening on UDP %d\n", UDP_PORT);
    resetMatch();

    const float DT = 1.0f / PHYS_HZ;
    const int   TICKS_PER_STATE = PHYS_HZ / NET_HZ;  // 3
    uint32_t    stateSeq  = 0;
    int         tickCount = 0;
    float       overTimer = 0.0f;

    auto next = std::chrono::steady_clock::now();
    while (true) {
        handlePackets(fd);
        tick(DT);

        for (int i = 0; i < 2; i++) {
            if (!clients[i].used) continue;
            clients[i].silence += DT;
            if (clients[i].silence > 5.0f) dropClient(i);
        }

        if (game.gameOver) {
            overTimer += DT;
            if (overTimer >= 5.0f) { overTimer = 0.0f; resetMatch(); }
        } else {
            overTimer = 0.0f;
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
