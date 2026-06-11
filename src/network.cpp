#include "network.h"
#include <cstdio>
#include <cstring>

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
    return true;
}

void ClientNet::update(float dt) {
    if (fd < 0) return;

    if (connecting && !connected) {
        helloTimer += dt;
        if (helloTimer >= 0.5f) {
            HelloPacket h{PKT_HELLO};
            netSend(fd, &h, sizeof(h), server);
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

    char buf[1024];
    sockaddr_in from{};
    int n;
    while ((n = netRecv(fd, buf, sizeof(buf), from)) > 0) {
        if (!netSameAddr(from, server)) continue;
        silence = 0.0f;
        PacketType type = (PacketType)buf[0];

        if (type == PKT_ACCEPT && n >= (int)sizeof(AcceptPacket)) {
            AcceptPacket a;
            memcpy(&a, buf, sizeof(a));
            playerID   = a.playerID;
            connected  = true;
            connecting = false;
            printf("net: connected as player %d\n", playerID);
        } else if (type == PKT_STATE && n >= (int)sizeof(StatePacket)) {
            if (hasState) {
                prevState = lastState;
                hasPrev   = true;
            }
            memcpy(&lastState, buf, sizeof(lastState));
            hasState   = true;
            sinceState = 0.0f;
        } else if (type == PKT_BYE) {
            printf("net: server closed connection\n");
            disconnect();
            return;
        }
    }
}

void ClientNet::sendInput(const InputState& in) {
    if (!connected) return;
    InputPacket p{};
    p.type = PKT_INPUT;
    p.seq  = ++inputSeq;
    p.keys = (in.w ? KEY_W : 0) | (in.a ? KEY_A : 0) |
             (in.s ? KEY_S : 0) | (in.d ? KEY_D : 0) |
             (in.shoot ? KEY_SHOOT : 0);
    p.yaw   = in.yaw;
    p.pitch = in.pitch;
    netSend(fd, &p, sizeof(p), server);
}

void unpackState(const StatePacket& a, const StatePacket& b, float alpha, GameState& out) {
    glm::vec3 p0a{a.p0x, a.p0y, a.p0z}, p0b{b.p0x, b.p0y, b.p0z};
    glm::vec3 p1a{a.p1x, a.p1y, a.p1z}, p1b{b.p1x, b.p1y, b.p1z};

    out.players[0].pos   = glm::mix(p0a, p0b, alpha);
    out.players[0].yaw   = a.p0yaw + (b.p0yaw - a.p0yaw) * alpha;
    out.players[0].hp    = b.p0hp;
    out.players[0].alive = b.p0hp > 0;

    out.players[1].pos   = glm::mix(p1a, p1b, alpha);
    out.players[1].yaw   = a.p1yaw + (b.p1yaw - a.p1yaw) * alpha;
    out.players[1].hp    = b.p1hp;
    out.players[1].alive = b.p1hp > 0;

    int count = b.bulletCount <= MAX_BULLETS ? b.bulletCount : MAX_BULLETS;
    for (int i = 0; i < MAX_BULLETS; i++) out.bullets[i].active = false;
    for (int i = 0; i < count; i++) {
        glm::vec3 pb{b.bullets[i].x, b.bullets[i].y, b.bullets[i].z};
        glm::vec3 pos = pb;
        if (i < a.bulletCount) {
            glm::vec3 pa{a.bullets[i].x, a.bullets[i].y, a.bullets[i].z};
            pos = glm::mix(pa, pb, alpha);
        }
        out.bullets[i].pos     = pos;
        out.bullets[i].active  = true;
        out.bullets[i].ownerID = -1;
    }
    out.bulletCount = count;
    out.gameOver    = b.gameOver != 0;
    out.winnerID    = b.winnerID;
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
