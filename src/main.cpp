// Client: offline practice vs dummy by default; connects to dedicated server
// via `./game <ip>` or the C key (IP prompt on stdin). Drop-in FFA, 16 players.
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

// Bar anchored at its left edge; fill scales with frac
static void drawBar(Renderer& r, glm::vec2 center, glm::vec2 size, float frac,
                    const glm::vec3& fillColor) {
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    r.drawRect(center, size, {0.1f, 0.1f, 0.1f}, 0.7f);
    float left = center.x - size.x * 0.5f;
    glm::vec2 fillCenter = {left + size.x * frac * 0.5f, center.y};
    r.drawRect(fillCenter, {size.x * frac, size.y * 0.7f}, fillColor, 0.9f);
}

static void drawHUD(Renderer& r, const GameState& gs, int localID, float flashAlpha) {
    float ia = 1.0f / r.aspect();  // keep HUD shapes square in NDC
    const Player& own = gs.players[localID];
    r.beginHUD();

    if (flashAlpha > 0.0f)
        r.drawRect({0, 0}, {2, 2}, {0.9f, 0.1f, 0.1f}, flashAlpha);

    float hpFrac = own.hp / (float)PLAYER_HP;
    glm::vec3 hpColor = glm::mix(glm::vec3(0.9f, 0.2f, 0.1f),
                                 glm::vec3(0.2f, 0.85f, 0.2f), hpFrac);
    drawBar(r, {-0.6f, -0.88f}, {0.5f, 0.05f}, hpFrac, hpColor);
    drawBar(r, {-0.6f, -0.95f}, {0.5f, 0.03f}, own.ammo / (float)AMMO_PER_LIFE,
            {0.95f, 0.85f, 0.25f});

    r.drawRect({0, 0}, {0.006f * ia, 0.045f}, {1, 1, 1}, 0.9f);  // crosshair |
    r.drawRect({0, 0}, {0.045f * ia, 0.006f}, {1, 1, 1}, 0.9f);  // crosshair -

    if (!own.alive)
        r.drawRect({0, 0}, {2, 2}, {0.6f, 0.05f, 0.05f}, 0.4f);  // dead, waiting respawn

    r.endHUD();
}

static void renderScene(Renderer& r, const Camera& cam, const GameState& gs, int localID,
                        float flashAlpha) {
    r.beginFrame(cam.view(), cam.proj(r.aspect()));
    r.drawGround();

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == localID) continue;
        if (!(gs.usedMask & (1u << i))) continue;
        if (!gs.players[i].alive) continue;
        r.drawCube(gs.players[i].pos + glm::vec3(0, 1.0f, 0), {1, 2, 1}, COLOR_ENEMY);
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet& b = gs.bullets[i];
        if (!b.active) continue;
        bool own = b.ownerID == localID;
        r.drawCube(b.pos, {0.1f, 0.1f, 0.1f}, own ? COLOR_BULLET_OWN : COLOR_BULLET_ENEMY);
    }
    drawHUD(r, gs, localID, flashAlpha);
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

    static GameState offline;             // local practice match vs dummy
    offline.usedMask = 0b11;              // slot 0 = self, slot 1 = dummy
    offline.players[1].pos = {0, 0, -10};
    static GameState display;             // what gets rendered when online

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
    float       flashTimer  = 0.0f;
    int         prevOwnHP   = PLAYER_HP;
    bool        prevOwnAlive = true;

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
                if (prevOwnAlive) movePlayer(predicted, input.state, FIXED_DT);
            } else {
                Player& self  = offline.players[0];
                Player& dummy = offline.players[1];
                if (shootPending && self.alive) {
                    spawnBullet(offline, cam.eye, cam.front(), 0);
                    if (self.ammo <= 0) self.ammo = AMMO_PER_LIFE;  // offline auto-refill
                    shootPending = false;
                }
                movePlayer(self, input.state, FIXED_DT);
                updateBullets(offline, FIXED_DT);
                if (!dummy.alive) {                                 // offline dummy respawn
                    dummy.respawnTimer -= FIXED_DT;
                    if (dummy.respawnTimer <= 0.0f) {
                        dummy = Player{};
                        dummy.pos = {0, 0, -10};
                    }
                }
            }
            accumulator -= FIXED_DT;
        }

        const GameState* shown = &offline;
        if (online) {
            // snap prediction to authoritative position on each new state
            if (net.lastState.seq != appliedStateSeq) {
                appliedStateSeq = net.lastState.seq;
                const PlayerNetState& own = net.lastState.players[net.playerID];
                predicted.pos = {own.x, own.y, own.z};
            }
            const StatePacket& a = net.hasPrev ? net.prevState : net.lastState;
            float alpha = net.sinceState * NET_HZ;
            if (alpha > 1.0f) alpha = 1.0f;
            unpackState(a, net.lastState, alpha, display);
            display.players[localID].pos = predicted.pos;  // own view from prediction
            shown = &display;
        }

        cam.eye = shown->players[localID].pos + glm::vec3(0, EYE_HEIGHT, 0);

        const Player& own = shown->players[localID];
        if (own.hp < prevOwnHP && own.alive) flashTimer = 0.4f;  // got hit
        prevOwnHP = own.hp;
        flashTimer -= dt;
        if (flashTimer < 0.0f) flashTimer = 0.0f;

        if (prevOwnAlive && !own.alive) printf("you died — respawning\n");
        if (!prevOwnAlive && own.alive) printf("respawned\n");
        prevOwnAlive = own.alive;

        renderScene(renderer, cam, *shown, localID, 0.35f * flashTimer / 0.4f);
    }

    net.disconnect();
    renderer.shutdown();
    SDL_Quit();
    platformSocketCleanup();
    return 0;
}
