// Client: offline practice vs dummy by default; connects to dedicated server
// via `./game <ip>` or the C key (IP prompt on stdin).
#include <SDL.h>
#include <cstdio>
#include "platform.h"
#include "renderer.h"
#include "camera.h"
#include "input.h"
#include "physics.h"
#include "network.h"

static const glm::vec3 COLOR_ENEMY        = {0.80f, 0.30f, 0.20f};
static const glm::vec3 COLOR_BULLET_OWN   = {1.00f, 0.90f, 0.20f};
static const glm::vec3 COLOR_BULLET_ENEMY = {1.00f, 0.40f, 0.10f};

static void renderScene(Renderer& r, const Camera& cam, const GameState& gs, int localID) {
    r.beginFrame(cam.view(), cam.proj(r.aspect()));
    r.drawGround();

    const Player& remote = gs.players[1 - localID];
    if (remote.alive) {
        r.drawCube(remote.pos + glm::vec3(0, 1.0f, 0), {1, 2, 1}, COLOR_ENEMY);
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet& b = gs.bullets[i];
        if (!b.active) continue;
        bool own = b.ownerID == localID;
        r.drawCube(b.pos, {0.1f, 0.1f, 0.1f}, own ? COLOR_BULLET_OWN : COLOR_BULLET_ENEMY);
    }
    r.endFrame();
}

static bool promptConnect(ClientNet& net) {
    char ip[64] = {0};
    SDL_SetRelativeMouseMode(SDL_FALSE);
    printf("server IP: ");
    fflush(stdout);
    if (scanf("%63s", ip) != 1) ip[0] = '\0';
    SDL_SetRelativeMouseMode(SDL_TRUE);
    if (!ip[0]) return false;
    printf("connecting to %s...\n", ip);
    return net.connect(ip);
}

int main(int argc, char** argv) {
    platformSocketInit();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    Renderer renderer;
    if (!renderer.init("pvp_shooter", 1280, 720)) {
        fprintf(stderr, "renderer init failed\n");
        return 1;
    }
    SDL_SetRelativeMouseMode(SDL_TRUE);

    GameState offline;                    // local practice match vs dummy
    offline.players[1].pos = {0, 0, -10};
    GameState display;                    // what gets rendered when online

    ClientNet net;
    Player    predicted;                  // own player, client-side predicted
    uint32_t  appliedStateSeq = 0;

    Camera     cam;
    FrameInput input;

    if (argc > 1) {
        printf("connecting to %s...\n", argv[1]);
        net.connect(argv[1]);
    }

    const float FIXED_DT    = 1.0f / PHYS_HZ;
    float       accumulator = 0.0f;
    Uint64      last        = SDL_GetPerformanceCounter();
    bool        running     = true;
    int         lastPrintedHP[2] = {PLAYER_HP, PLAYER_HP};
    bool        winPrinted  = false;

    printf("controls: WASD move, mouse look, LMB shoot, C connect, F wireframe, ESC quit\n");
    printf("offline practice mode until connected\n");

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = (float)(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (dt > 0.05f) dt = 0.05f;   // cap to avoid spiral

        pollInput(input, cam);
        if (input.quit) running = false;
        if (input.wireframeToggle) renderer.toggleWireframe();
        if (input.connectRequested && !net.connected && !net.connecting) {
            promptConnect(net);
            last = SDL_GetPerformanceCounter();  // stdin blocked; don't count it as dt
        }

        net.update(dt);

        bool online  = net.connected && net.hasState;
        int  localID = online ? net.playerID : 0;

        if (net.connected) net.sendInput(input.state);

        // fixed-step simulation
        accumulator += dt;
        bool shootPending = input.state.shoot;
        while (accumulator >= FIXED_DT) {
            if (online) {
                movePlayer(predicted, input.state, FIXED_DT);  // prediction only
            } else if (!offline.gameOver) {
                if (shootPending) {
                    spawnBullet(offline, cam.eye, cam.front(), 0);
                    shootPending = false;
                }
                movePlayer(offline.players[0], input.state, FIXED_DT);
                updateBullets(offline, FIXED_DT);
            }
            accumulator -= FIXED_DT;
        }

        const GameState* shown = &offline;
        if (online) {
            // snap prediction to authoritative position on each new state
            if (net.lastState.seq != appliedStateSeq) {
                appliedStateSeq = net.lastState.seq;
                if (net.playerID == 0)
                    predicted.pos = {net.lastState.p0x, net.lastState.p0y, net.lastState.p0z};
                else
                    predicted.pos = {net.lastState.p1x, net.lastState.p1y, net.lastState.p1z};
            }
            const StatePacket& a = net.hasPrev ? net.prevState : net.lastState;
            float alpha = net.sinceState * NET_HZ;
            if (alpha > 1.0f) alpha = 1.0f;
            unpackState(a, net.lastState, alpha, display);
            display.players[localID].pos = predicted.pos;  // own view from prediction
            shown = &display;
        }

        cam.eye = shown->players[localID].pos + glm::vec3(0, EYE_HEIGHT, 0);

        int ownHP   = shown->players[localID].hp;
        int enemyHP = shown->players[1 - localID].hp;
        if (ownHP != lastPrintedHP[0] || enemyHP != lastPrintedHP[1]) {
            lastPrintedHP[0] = ownHP;
            lastPrintedHP[1] = enemyHP;
            printf("HP: you %d | enemy %d\n", ownHP, enemyHP);
        }
        if (shown->gameOver && !winPrinted) {
            winPrinted = true;
            printf("game over — you %s\n", shown->winnerID == localID ? "WIN" : "LOSE");
        }
        if (!shown->gameOver) winPrinted = false;  // server reset the match

        renderScene(renderer, cam, *shown, localID);
    }

    net.disconnect();
    renderer.shutdown();
    SDL_Quit();
    platformSocketCleanup();
    return 0;
}
