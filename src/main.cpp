// Client: offline practice vs dummy by default; connects to dedicated server
// via `./game <ip>` or the C key (IP prompt on stdin). Drop-in FFA, 16 players.
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <glm/gtc/matrix_transform.hpp>
#include "platform.h"
#include "renderer.h"
#include "camera.h"
#include "input.h"
#include "physics.h"
#include "network.h"
#include "map.h"
#include "hud.h"

static const glm::vec3 COLOR_ENEMY        = {0.80f, 0.30f, 0.20f};
static const glm::vec3 COLOR_BULLET_OWN   = {1.00f, 0.90f, 0.20f};
static const glm::vec3 COLOR_BULLET_ENEMY = {1.00f, 0.40f, 0.10f};

// First-person weapon state (client cosmetic; gameplay is server-authoritative).
struct ViewModel {
    float adsT       = 0.0f;  // 0 = hipfire pose, 1 = aimed
    float flashTimer = 0.0f;  // muzzle flash seconds remaining
    float recoilT    = 0.0f;  // recoil kick, 1 on fire, decays to 0
};

// Draws the held gun from a few oriented cubes, anchored to the camera and
// lerped between hipfire (lower-right) and ADS (centered under crosshair).
static void drawViewModel(Renderer& r, const Camera& cam, const ViewModel& vm) {
    glm::vec3 front = cam.front();
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
    glm::vec3 up    = glm::cross(right, front);

    glm::vec3 hipOff = right * 0.17f - up * 0.15f + front * 0.35f;
    glm::vec3 adsOff =                - up * 0.05f + front * 0.30f;
    glm::vec3 off    = glm::mix(hipOff, adsOff, vm.adsT);
    glm::vec3 anchor = cam.eye + off - front * (vm.recoilT * 0.08f);

    glm::mat4 basis(glm::vec4(right, 0), glm::vec4(up, 0),
                    glm::vec4(front, 0), glm::vec4(0, 0, 0, 1));
    glm::mat4 anchorM = glm::translate(glm::mat4(1.0f), anchor) * basis;
    auto part = [&](glm::vec3 lp, glm::vec3 sz, glm::vec3 col) {
        glm::mat4 m = glm::scale(glm::translate(anchorM, lp), sz);
        r.drawCubeModel(m, col);
    };
    const glm::vec3 metal = {0.12f, 0.12f, 0.14f};
    const glm::vec3 dark  = {0.20f, 0.20f, 0.23f};
    part({0.0f,  0.00f,  0.00f}, {0.07f,  0.09f,  0.30f}, metal);  // body
    part({0.0f,  0.02f,  0.27f}, {0.035f, 0.035f, 0.28f}, dark);   // barrel
    part({0.0f, -0.11f, -0.04f}, {0.05f,  0.14f,  0.07f}, metal);  // grip

    if (vm.flashTimer > 0.0f) {                                    // muzzle flash
        glm::vec3 muzzle = anchor + up * 0.02f + front * 0.44f;
        r.drawCube(muzzle, glm::vec3(0.16f), {1.0f, 0.85f, 0.35f});
    }
}

static void renderScene(Renderer& r, const Camera& cam, const GameState& gs, int localID,
                        const HudState& hud, bool scoreboard, bool online,
                        const ViewModel& vm) {
    static const glm::vec3 COLOR_BLOB = {0.16f, 0.27f, 0.16f};  // ground, darkened

    r.beginFrame(cam.view(), cam.proj(r.aspect()), cam.eye);
    r.drawGround();

    for (int i = 0; i < MAP_BOX_COUNT; i++) {
        const Box& b = MAP_BOXES[i];
        r.drawCube(b.center, b.half * 2.0f, {0.45f, 0.45f, 0.52f});
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == localID) continue;
        if (!(gs.usedMask & (1u << i))) continue;
        if (!gs.players[i].alive) continue;
        const glm::vec3& p = gs.players[i].pos;
        float bodyH = gs.players[i].crouched ? CROUCH_HEIGHT : STAND_HEIGHT;
        r.drawCube(p + glm::vec3(0, bodyH * 0.5f, 0), {1, bodyH, 1}, COLOR_ENEMY);
        r.drawCube({p.x, 0.01f, p.z}, {1.1f, 0.001f, 1.1f}, COLOR_BLOB);
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet& b = gs.bullets[i];
        if (!b.active) continue;
        bool own = b.ownerID == localID;
        r.drawCube(b.pos, {0.1f, 0.1f, 0.1f}, own ? COLOR_BULLET_OWN : COLOR_BULLET_ENEMY);
        r.drawCube({b.pos.x, 0.01f, b.pos.z}, {0.22f, 0.001f, 0.22f}, COLOR_BLOB);
    }
    if (gs.players[localID].alive) drawViewModel(r, cam, vm);
    drawHUD(r, gs, localID, hud, scoreboard, online);
    r.endFrame();
}

// Debug: FPS_SHOT=<path.ppm> dumps the framebuffer once, shortly after start.
// Drawable size, not window size — they differ on HiDPI displays.
static void dumpFrame(const Renderer& r, const char* path) {
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(r.window, &w, &h);
    unsigned char* px = (unsigned char*)malloc((size_t)w * h * 3);  // one-shot debug path
    if (!px) return;
    glReadBuffer(GL_FRONT);  // called after the swap; back buffer is undefined
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    glReadBuffer(GL_BACK);
    FILE* f = fopen(path, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; y--)        // GL reads bottom-up
            fwrite(&px[(size_t)y * w * 3], 1, (size_t)w * 3, f);
        fclose(f);
        printf("frame dumped to %s\n", path);
    }
    free(px);
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
    offline.players[0].pos = {12, 0, 12}; // clear of the center pillar
    offline.players[1].pos = {3, 0, -3};
    static GameState display;             // what gets rendered when online

    ClientNet net;
    Player    predicted;                  // own player, client-side predicted
    uint32_t  appliedStateSeq = 0;

    Camera     cam;
    cam.yaw = 230.0f;  // offline spawn looks toward the arena center
    FrameInput input;

    if (argc > 1) {
        printf("connecting to %s...\n", argv[1]);
        net.connect(argv[1]);
    }

    const float FIXED_DT    = 1.0f / PHYS_HZ;
    float       accumulator = 0.0f;
    Uint64      last        = SDL_GetPerformanceCounter();
    bool        running     = true;
    HudState    hud;
    int         prevOwnHP   = PLAYER_HP;
    bool        prevOwnAlive = true;
    bool        offlineShoot = false;  // latch click until a physics tick consumes it
    ViewModel   vm;                     // first-person gun state

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
        if (online) offlineShoot = false;            // online sends shoot directly each frame
        else if (input.state.shoot) offlineShoot = true;  // survives frames with no physics tick
        while (accumulator >= FIXED_DT) {
            if (online) {
                if (prevOwnAlive) movePlayer(predicted, input.state, FIXED_DT);
            } else {
                Player& self  = offline.players[0];
                Player& dummy = offline.players[1];
                if (offlineShoot && self.alive) {
                    spawnBullet(offline, cam.eye, cam.front(), 0);
                    if (self.ammo <= 0) self.ammo = AMMO_PER_LIFE;  // offline auto-refill
                    offlineShoot = false;
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
                hud.noteState(net.lastState);
            }
            const StatePacket& a = net.hasPrev ? net.prevState : net.lastState;
            float alpha = net.sinceState * NET_HZ;
            if (alpha > 1.0f) alpha = 1.0f;
            unpackState(a, net.lastState, alpha, display);
            display.players[localID].pos = predicted.pos;  // own view from prediction
            shown = &display;
        }

        float eyeH = shown->players[localID].crouched ? CROUCH_EYE : EYE_HEIGHT;
        cam.eye = shown->players[localID].pos + glm::vec3(0, eyeH, 0);

        const Player& own = shown->players[localID];
        if (own.hp < prevOwnHP && own.alive) hud.flashTimer = 0.4f;  // got hit
        prevOwnHP = own.hp;
        hud.flashTimer -= dt;
        if (hud.flashTimer < 0.0f) hud.flashTimer = 0.0f;

        if (prevOwnAlive && !own.alive) {
            printf("you died — respawning\n");
            hud.deathTimer = RESPAWN_TIME;
        }
        if (!prevOwnAlive && own.alive) printf("respawned\n");
        prevOwnAlive = own.alive;
        hud.deathTimer -= dt;
        if (hud.deathTimer < 0.0f) hud.deathTimer = 0.0f;
        hud.feed.update(dt);

        // weapon viewmodel: ADS transition, FOV zoom, fire feedback
        float adsTarget = input.state.ads ? 1.0f : 0.0f;
        float k = dt * ADS_LERP_SPEED;
        if (k > 1.0f) k = 1.0f;
        vm.adsT += (adsTarget - vm.adsT) * k;
        cam.fov  = glm::mix(HIP_FOV, ADS_FOV, vm.adsT);
        if (input.state.shoot && own.alive && own.ammo > 0) {
            vm.flashTimer = 0.05f;
            vm.recoilT    = 1.0f;
        }
        vm.flashTimer -= dt;
        if (vm.flashTimer < 0.0f) vm.flashTimer = 0.0f;
        vm.recoilT -= dt * 8.0f;
        if (vm.recoilT < 0.0f) vm.recoilT = 0.0f;

        renderScene(renderer, cam, *shown, localID, hud, input.scoreboardHeld, online, vm);

        static int frameCount = 0;
        const char* shotPath = getenv("FPS_SHOT");
        if (shotPath && ++frameCount == 60) dumpFrame(renderer, shotPath);
    }

    net.disconnect();
    renderer.shutdown();
    SDL_Quit();
    platformSocketCleanup();
    return 0;
}
