// Client: offline practice vs dummy by default; connects to dedicated server
// via `./game <ip>` or the C key (IP prompt on stdin). Drop-in FFA, 16 players.
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include "platform.h"
#include "renderer.h"
#include "camera.h"
#include "input.h"
#include "physics.h"
#include "network.h"
#include "map.h"
#include "hud.h"
#include "audio.h"

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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    Renderer renderer;
    if (!renderer.init("pvp_shooter", 1280, 720)) {
        fprintf(stderr, "renderer init failed\n");
        return 1;
    }
    SDL_SetRelativeMouseMode(SDL_TRUE);

    Audio audio;
    audio.init();   // runs silent if no device

    static GameState offline;             // local practice match vs dummy
    offline.usedMask = 0b11;              // slot 0 = self, slot 1 = dummy
    offline.players[0].pos = {12, 0, 12}; // clear of the center pillar
    offline.players[1].pos = {3, 0, -3};
    static GameState display;             // what gets rendered when online

    ClientNet net;
    Player    predicted;                  // own player, client-side predicted
    uint32_t  appliedStateSeq = 0;
    glm::vec3 posError(0.0f);             // smooths the snap to authoritative pos
    const float PRED_SMOOTH_TAU = 0.08f;  // correction half-life (~smoothing window)

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
    bool        prevJumpKey  = false;   // for jump-sound edge
    float       stepTimer    = 0.0f;    // footstep cadence
    float       prevOwnPosY  = 0.0f;    // detect airborne (no footsteps in air)
    bool        heardBullet[MAX_BULLETS] = {false};  // enemy-shot sound: bullets already sounded
    float       fpsAvg       = 0.0f;    // smoothed FPS readout
    int         fireMode     = FIRE_SEMI;
    float       fireTimer    = 0.0f;    // cooldown until next allowed shot
    int         burstRemaining = 0;     // rounds left in current burst
    bool        prevReloading = false;  // for reload-start sound
    uint8_t     prevOwnHits  = 0;       // hit-marker: own hits-dealt counter
    glm::vec3   hitMarkerPos(0.0f);     // world impact point of the latest hit
    float       recoilHeat   = 0.0f;    // ramps recoil kick over a sustained spray
    float       sinceShot    = 1e9f;    // seconds since last shot (recovery gating)

    printf("controls: WASD move, mouse look, LMB shoot, C connect, F wireframe, ESC quit\n");
    printf("offline practice mode until connected\n");

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = (float)(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (dt > 0.0f) {              // smoothed FPS from raw frame time (before the cap)
            float inst = 1.0f / dt;
            fpsAvg = fpsAvg <= 0.0f ? inst : fpsAvg + (inst - fpsAvg) * 0.1f;
            hud.fps = fpsAvg;
        }
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

        // --- fire control: pick shots this frame by fire mode, gate on ammo ---
        if (input.fireModeToggle) { fireMode = (fireMode + 1) % FIRE_MODE_COUNT; burstRemaining = 0; }
        hud.fireMode = fireMode;

        bool ownAliveF, ownReloadingF; int ownMagF;   // own weapon state for gating
        if (online) {
            const PlayerNetState& o = net.lastState.players[localID];
            ownAliveF = o.alive != 0; ownMagF = o.mag; ownReloadingF = o.reloading != 0;
        } else {
            const Player& s = offline.players[0];
            ownAliveF = s.alive; ownMagF = s.mag; ownReloadingF = s.reloading;
        }

        fireTimer -= dt;
        if (fireMode == FIRE_BURST && input.state.shoot) burstRemaining = BURST_COUNT;
        bool  wantFire = false;
        float fireInt  = FIRE_SEMI_INT;
        if (fireMode == FIRE_SEMI)  { wantFire = input.state.shoot;     fireInt = FIRE_SEMI_INT;  }
        if (fireMode == FIRE_BURST) { wantFire = burstRemaining > 0;    fireInt = FIRE_BURST_INT; }
        if (fireMode == FIRE_AUTO)  { wantFire = input.state.shootHeld; fireInt = FIRE_AUTO_INT;  }

        bool fired = wantFire && ownAliveF && ownMagF > 0 && !ownReloadingF && fireTimer <= 0.0f;
        if (online) offlineShoot = false;
        if (fired) {
            fireTimer = fireInt;
            if (fireMode == FIRE_BURST && burstRemaining > 0) burstRemaining--;
            if (online) net.shotSeq++;   // reliable shot; server spawns + decrements mag
            else        offlineShoot = true;
            vm.flashTimer = 0.05f;
            vm.recoilT    = 1.0f;
            audioPlay(SND_SHOOT);
        }
        if (input.state.shoot && ownAliveF && ownMagF == 0 && !ownReloadingF) audioPlay(SND_DRYFIRE);
        if (ownReloadingF && !prevReloading) audioPlay(SND_RELOAD);
        prevReloading = ownReloadingF;

        // Report input (incl. this frame's shotSeq) with the pre-kick aim, so the
        // shot leaves the barrel exactly where aimed; the kick lands afterwards.
        if (net.connected) {
            uint32_t viewSeq  = net.hasState ? net.lastState.seq : 0;
            uint8_t  viewFrac = 0;
            if (net.hasPrev) {
                viewSeq = net.prevState.seq;          // interpolating prev -> last
                float a = net.sinceState * NET_HZ;
                if (a < 0.0f) a = 0.0f;
                if (a > 1.0f) a = 1.0f;
                viewFrac = (uint8_t)(a * 255.0f + 0.5f);
            }
            net.sendInput(input.state, viewSeq, viewFrac);
        }

        // Recoil: kick the aim up + a little sideways, escalating with heat. Applied
        // after the shot is sent so each shot carries only prior shots' kicks.
        if (fired) {
            float t    = recoilHeat / RECOIL_HEAT_CAP;
            float mult = input.state.ads ? RECOIL_ADS_MULT : RECOIL_HIP_MULT;
            if (recoilHeat == 0.0f) mult *= RECOIL_FIRST_MULT;  // snappier opening shot
            float rs   = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;  // [-1,1]
            float vKick = glm::mix(RECOIL_PITCH_MIN, RECOIL_PITCH_MAX, t) * mult;
            float hKick = RECOIL_YAW * (0.6f + 0.8f * t) * mult * rs;
            cam.applyRecoil(vKick, hKick);
            recoilHeat += 1.0f;
            if (recoilHeat > RECOIL_HEAT_CAP) recoilHeat = RECOIL_HEAT_CAP;
            sinceShot = 0.0f;
        }
        sinceShot += dt;
        if (sinceShot > RECOIL_HEAT_RESET) recoilHeat = 0.0f;
        cam.recoverRecoil(dt, sinceShot < RECOIL_RECOVER_DELAY);

        // fixed-step simulation
        accumulator += dt;
        while (accumulator >= FIXED_DT) {
            if (online) {
                if (prevOwnAlive) movePlayer(predicted, input.state, FIXED_DT);
            } else {
                Player& self  = offline.players[0];
                Player& dummy = offline.players[1];
                if (offlineShoot && self.alive) {
                    glm::vec3 origin, dir;
                    float spread = aimSpread(self, input.state);
                    weaponShot(input.state.yaw, input.state.pitch, cam.eye, input.state.ads,
                               spread, origin, dir);
                    spawnBullet(offline, origin, dir, 0);
                    offlineShoot = false;
                }
                movePlayer(self, input.state, FIXED_DT);
                updateReload(self, input.state.reload, FIXED_DT);
                if (self.mag == 0 && self.reserve == 0) self.reserve = RESERVE_PER_LIFE;  // keep practice stocked
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
            // Reconcile prediction with each new authoritative state. The server
            // position trails our prediction by ~RTT, so snapping straight to it
            // jolts the camera under latency. Instead keep the corrected position
            // for the sim, but roll the jump into posError and decay it out so the
            // render eases over a few frames while local input stays responsive.
            if (net.lastState.seq != appliedStateSeq) {
                appliedStateSeq = net.lastState.seq;
                const PlayerNetState& own = net.lastState.players[net.playerID];
                glm::vec3 authoritative = {own.x, own.y, own.z};
                posError += predicted.pos - authoritative;
                predicted.pos = authoritative;
                if (glm::length(posError) > 2.0f) posError = glm::vec3(0.0f);  // big desync: snap
                hud.noteState(net.lastState);
            }
            const StatePacket& a = net.hasPrev ? net.prevState : net.lastState;
            float alpha = net.sinceState * NET_HZ;
            if (alpha > 1.0f) alpha = 1.0f;
            unpackState(a, net.lastState, alpha, display);
            display.players[localID].pos = predicted.pos + posError;  // smoothed own view
            shown = &display;
        }
        posError *= expf(-dt / PRED_SMOOTH_TAU);
        if (glm::length(posError) < 0.0005f) posError = glm::vec3(0.0f);

        float eyeH = shown->players[localID].crouched ? CROUCH_EYE : EYE_HEIGHT;
        cam.eye = shown->players[localID].pos + glm::vec3(0, eyeH, 0);

        const Player& own = shown->players[localID];
        if (own.hp < prevOwnHP && own.alive) hud.flashTimer = 0.4f;  // got hit
        prevOwnHP = own.hp;
        hud.flashTimer -= dt;
        if (hud.flashTimer < 0.0f) hud.flashTimer = 0.0f;

        // Hit marker: arm when our own hits-dealt counter advances, then keep the
        // world impact point projected to screen for the marker's lifetime. The
        // delta < 128 test ignores counter resets/wraps (respawn, offline->online).
        uint8_t curHits = online ? net.lastState.recvHits : (uint8_t)offline.players[0].hits;
        glm::vec3 impact = online
            ? glm::vec3(net.lastState.recvHitX, net.lastState.recvHitY, net.lastState.recvHitZ)
            : offline.players[0].lastHitPos;
        uint8_t hitDelta = (uint8_t)(curHits - prevOwnHits);
        if (hitDelta != 0 && hitDelta < 128) {
            hitMarkerPos       = impact;
            hud.hitMarkerTimer = HIT_MARKER_TIME;
        }
        prevOwnHits = curHits;
        if (hud.hitMarkerTimer > 0.0f) {
            glm::vec4 clip = cam.proj(renderer.aspect()) * cam.view() * glm::vec4(hitMarkerPos, 1.0f);
            hud.hitMarkerOnScreen = clip.w > 1e-4f;
            if (hud.hitMarkerOnScreen) hud.hitMarkerNDC = {clip.x / clip.w, clip.y / clip.w};
        }
        hud.hitMarkerTimer -= dt;
        if (hud.hitMarkerTimer < 0.0f) hud.hitMarkerTimer = 0.0f;

        if (prevOwnAlive && !own.alive) {
            printf("you died — respawning\n");
            hud.deathTimer = RESPAWN_TIME;
            audioPlay(SND_DEATH);
        }
        if (!prevOwnAlive && own.alive) {
            printf("respawned\n");
            audioPlay(SND_RESPAWN);
        }
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
        vm.flashTimer -= dt;
        if (vm.flashTimer < 0.0f) vm.flashTimer = 0.0f;
        vm.recoilT -= dt * 8.0f;
        if (vm.recoilT < 0.0f) vm.recoilT = 0.0f;

        // jump sound on key press; footsteps while moving on the ground
        bool jumpEdge = input.state.jump && !prevJumpKey;
        if (jumpEdge && own.alive) audioPlay(SND_JUMP, 0.7f);
        prevJumpKey = input.state.jump;

        float dY      = own.pos.y - prevOwnPosY;
        bool  onGround = dY < 0.02f && dY > -0.02f;       // not climbing/falling
        bool  moving   = input.state.w || input.state.a || input.state.s || input.state.d;
        if (own.alive && moving && onGround) {
            stepTimer -= dt;
            if (stepTimer <= 0.0f) {
                audioPlay(SND_STEP, input.state.crouch ? 0.4f : 0.7f);
                stepTimer = input.state.sprint ? 0.28f : input.state.crouch ? 0.5f : 0.38f;
            }
        } else {
            stepTimer = 0.0f;                              // first step fires instantly
        }
        prevOwnPosY = own.pos.y;

        // enemy gunshots: a bullet pool slot newly appearing = someone fired.
        // Track by stable poolIdx from the raw state; volume falls off with distance.
        if (online) {
            bool nowSeen[MAX_BULLETS] = {false};
            const StatePacket& st = net.lastState;
            int bc = st.bulletCount <= NET_MAX_BULLETS ? st.bulletCount : NET_MAX_BULLETS;
            for (int k = 0; k < bc; k++) {
                const BulletNetState& nb = st.bullets[k];  // poolIdx is uint8_t, always < MAX_BULLETS (256)
                nowSeen[nb.poolIdx] = true;
                if (!heardBullet[nb.poolIdx] && nb.owner != localID) {
                    float d   = glm::length(glm::vec3(nb.x, nb.y, nb.z) - cam.eye);
                    float vol = 1.0f - d / 60.0f;
                    if (vol > 0.08f) audioPlay(SND_SHOOT, vol > 1.0f ? 1.0f : vol);
                }
            }
            for (int i = 0; i < MAX_BULLETS; i++) heardBullet[i] = nowSeen[i];
        } else {
            for (int i = 0; i < MAX_BULLETS; i++) heardBullet[i] = false;
        }

        renderScene(renderer, cam, *shown, localID, hud, input.scoreboardHeld, online, vm);

        static int frameCount = 0;
        const char* shotPath = getenv("FPS_SHOT");
        if (shotPath && ++frameCount == 60) dumpFrame(renderer, shotPath);
    }

    net.disconnect();
    audio.shutdown();
    renderer.shutdown();
    SDL_Quit();
    platformSocketCleanup();
    return 0;
}
