// Client: starts in training mode (offline) by default. Auto-connects only with
// `./game <ip>`. Press C for an in-game IP prompt (127.0.0.1 pre-filled).
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include "platform.h"
#include "renderer.h"
#include "camera.h"
#include "input.h"
#include "physics.h"
#include "network.h"
#include "map.h"
#include "stand_mesh.h"
#include "hud.h"
#include "audio.h"
#include "material.h"
#include "connect_prompt.h"
#include "lobby.h"
#include "skeleton.h"
#include "ragdoll.h"
#include "weapon_visual.h"
#include "playerpose.h"
#include "player_visual.h"
#include "perf.h"

static const glm::vec3 COLOR_ENEMY        = {0.80f, 0.30f, 0.20f};
static const glm::vec3 COLOR_BULLET_OWN   = {1.00f, 0.90f, 0.20f};
static const glm::vec3 COLOR_BULLET_ENEMY = {1.00f, 0.40f, 0.10f};

// Persistent bullet-impact decal: a small patch laid flat on whatever surface the round
// struck (ground, cover, walls). Stamped from the local sim offline and from the
// server's PKT_IMPACT online, so every world detail shows hits, not just the range wall.
struct Decal { glm::vec3 pos; glm::vec3 normal; };
static const glm::vec3 COLOR_DECAL_HOLE = {0.05f, 0.04f, 0.03f};
static const glm::vec3 COLOR_DECAL_HALO = {0.52f, 0.40f, 0.24f};
// Axis-aligned normals, indexed by the protocol's ImpactDir code (IMP_PX..IMP_NZ).
static const glm::vec3 IMPACT_DIRS[6] = {
    {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1},
};

constexpr int DECAL_MAX = 512;    // ring buffer of impact decals

// A flat impact patch oriented to the surface normal: a thin box (normal axis short,
// the other two ~decal-sized) nudged just off the surface so it reads as a mark.
static void drawDecal(Renderer& r, const Decal& d) {
    glm::vec3 n  = d.normal;
    glm::vec3 t  = fabsf(n.y) > 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    glm::vec3 bx = glm::normalize(glm::cross(t, n));
    glm::vec3 bz = glm::normalize(glm::cross(n, bx));
    // Columns ordered (bz, n, bx) so the basis is right-handed (det +1): with the
    // left-handed order the outward broad face winds backwards and back-face culling
    // drops it, leaving only the thin rim visible. n is the short (thickness) axis.
    float h = fabsf(sinf(d.pos.x * 12.9898f + d.pos.y * 37.719f + d.pos.z * 78.233f)
                    * 43758.5453f);
    h -= floorf(h);
    float a    = h * 6.2831853f;
    glm::vec3 rx = bx * cosf(a) + bz * sinf(a);
    glm::vec3 rz = bz * cosf(a) - bx * sinf(a);
    float halo = 0.020f + h * 0.014f;
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(rz * halo, 0.0f);
    m[1] = glm::vec4(n  * 0.003f, 0.0f);
    m[2] = glm::vec4(rx * (halo * (0.7f + h * 0.5f)), 0.0f);
    m[3] = glm::vec4(d.pos + n * 0.004f, 1.0f);   // sit proud of the surface (no z-fight)
    r.drawCubeModel(m, COLOR_DECAL_HALO);
    float hole = 0.009f;
    m[0] = glm::vec4(rz * hole, 0.0f);
    m[1] = glm::vec4(n  * 0.003f, 0.0f);
    m[2] = glm::vec4(rx * hole, 0.0f);
    m[3] = glm::vec4(d.pos + n * 0.007f, 1.0f);
    r.drawCubeModel(m, COLOR_DECAL_HOLE);
}

// Shadow casters + receivers: ground/terrain, cover boxes, remote players, bullets.
// Drawn twice per frame — once into the sun's depth map, once into the main view —
// so it must contain only real world geometry (no HUD, viewmodel, mirror, or the
// cosmetic ground blobs / aim cross, which are added in the main pass only).
static void drawWorldGeometry(Renderer& r, const GameState& gs, int localID,
                              const float* walkPhase, const float* walkAmp,
                              const float* crouchAnim, const float* adsAnim, bool showLocal,
                              const Ragdoll* ragdolls,
                              const Frustum& fr, const glm::vec3& eye) {
    if (gMapId == MAP_LOBBY) {
        // Flat pad + plain material boxes (target wall, test cover).
        r.drawTerrain(fr, eye);
        for (int i = 0; i < gMapBoxCount; i++) {
            if (i >= gStandBoxFirst && i < gStandBoxFirst + gStandBoxCount) continue;
            const Box& b = gMapBoxes[i];
            if (!fr.aabbVisible(b.center, b.half)) continue;
            r.drawCube(b.center, b.half * 2.0f, mapBoxMaterial(i));
        }
        glm::vec3 sp = {LOBBY_STAND_X, terrainHeight(LOBBY_STAND_X, LOBBY_STAND_Z),
                        LOBBY_STAND_Z};
        glm::vec3 sc = sp + glm::vec3(STAND_MIN[0] + STAND_MAX[0],
                                      STAND_MIN[1] + STAND_MAX[1],
                                      STAND_MIN[2] + STAND_MAX[2]) * 0.5f;
        glm::vec3 sh = glm::vec3(STAND_MAX[0] - STAND_MIN[0],
                                 STAND_MAX[1] - STAND_MIN[1],
                                 STAND_MAX[2] - STAND_MIN[2]) * 0.5f;
        if (fr.aabbVisible(sc, sh)) r.drawMesh(r.stand, sp, MAT_WOOD);
    } else {
        r.drawTerrain(fr, eye);
    }
    r.drawVegetation(fr, eye);   // taiga forest (miniature spruce ring on the lobby)
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == localID && !showLocal) continue;   // draw self in third-person test view
        if (!(gs.usedMask & (1u << i))) continue;
        if (!gs.players[i].alive) {
            if (ragdolls && ragdolls[i].active) ragdolls[i].draw(r);  // cosmetic corpse
            continue;
        }
        const Player& pl = gs.players[i];
        drawPlayerSkeleton(r, pl.pos, pl.yaw, pl.pitch, pl.lean, pl.weaponId, crouchAnim[i],
                           adsAnim[i], walkPhase[i], walkAmp[i], COLOR_ENEMY,
                           glm::dot(pl.pos - eye, pl.pos - eye) > 15.0f * 15.0f);
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet& b = gs.bullets[i];
        if (!b.active) continue;
        bool own = b.ownerID == localID;
        r.drawCube(b.pos, {0.1f, 0.1f, 0.1f}, own ? COLOR_BULLET_OWN : COLOR_BULLET_ENEMY);
    }
}

static void renderScene(Renderer& r, const Camera& cam, const GameState& gs, int localID,
                        const HudState& hud, bool scoreboard, bool online,
                        const ViewModel& vm,
                        const Decal* decals, int decalCount,
                        const ConnectPrompt& connectPrompt, const Lobby& lobby,
                        const float* walkPhase, const float* walkAmp,
                        const float* crouchAnim, const float* adsAnim,
                        bool showHitboxes, bool fullMap, bool showHud,
                        bool thirdPerson, const Ragdoll* ragdolls) {
    static const glm::vec3 COLOR_BLOB = {0.16f, 0.27f, 0.16f};  // ground, darkened

    gProfiler.beginFrame();
    r.invalidateWorldOnMapChange();   // drop town/tree/prop caches on map switch

    // Pass 1: scene depth from the sun, focused on the camera (near-field shadows).
    gProfiler.beginPass(PASS_SHADOW);
    r.beginShadowPass(cam.eye);
    Frustum sunFr = Frustum::fromVP(r.lightSpace);
    drawWorldGeometry(r, gs, localID, walkPhase, walkAmp, crouchAnim, adsAnim,
                      thirdPerson, ragdolls, sunFr, cam.eye);
    r.endShadowPass();
    gProfiler.endPass(PASS_SHADOW);

    // Pass 2: lit main view, sampling the shadow map built above.
    gProfiler.beginPass(PASS_SKY);
    r.beginFrame(cam.view(), cam.proj(r.aspect()), cam.eye);
    r.drawSky(cam.view(), cam.proj(r.aspect()), cam.eye);
    gProfiler.endPass(PASS_SKY);

    gProfiler.beginPass(PASS_WORLD);
    Frustum camFr = Frustum::fromVP(cam.proj(r.aspect()) * cam.view());
    drawWorldGeometry(r, gs, localID, walkPhase, walkAmp, crouchAnim, adsAnim,
                      thirdPerson, ragdolls, camFr, cam.eye);
    gProfiler.endPass(PASS_WORLD);

    gProfiler.beginPass(PASS_WATER);
    r.drawWater();   // translucent Baltic, after all opaque world geometry
    gProfiler.endPass(PASS_WATER);

    if (gMapId == MAP_LOBBY) {
        // Shooting-range aim reference: red cross on the target wall at eye height.
        glm::vec3 c = {LOBBY_WALL_FACE - 0.03f, LOBBY_BULLSEYE_Y, 0.0f};
        r.drawCube(c, {0.06f, 1.0f, 0.10f}, {0.85f, 0.25f, 0.20f});
        r.drawCube(c, {0.06f, 0.10f, 1.0f}, {0.85f, 0.25f, 0.20f});
    }
    // Bullet-impact decals on every surface (online + offline), oriented to the hit face.
    for (int i = 0; i < decalCount; i++)
        drawDecal(r, decals[i]);
    if (showHitboxes) {
        // Debug (H): translucent green gameplay hit regions overlaid on the models.
        // Dead players get the stack at their (frozen) death pos so the ragdoll can be
        // compared against the boxes — hits don't actually register on corpses.
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!(gs.usedMask & (1u << i))) continue;
            if (i == localID && !thirdPerson) continue;
            const Player& pl = gs.players[i];
            HitRegion rg[MAX_HIT_REGIONS];
            int nr = playerHitRegions(pl.pos, pl.crouched, pl.yaw, pl.pitch, pl.lean,
                                      pl.ads, pl.weaponId, rg);
            // Each region carries its own oriented frame — the exact volumes the
            // server sweeps, so what you see is what gets hit.
            for (int k = 0; k < nr; k++)
                r.drawCubeModelTranslucent(
                    rg[k].M * glm::scale(glm::mat4(1.0f), rg[k].half * 2.0f),
                    {0.20f, 0.90f, 0.30f}, 0.35f);
        }
    }
    // Bullet ground blobs (real shadows replace the old per-player blob).
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet& b = gs.bullets[i];
        if (!b.active) continue;
        r.drawCube({b.pos.x, terrainHeight(b.pos.x, b.pos.z) + 0.01f, b.pos.z},
                   {0.22f, 0.001f, 0.22f}, COLOR_BLOB);
    }
    // First-person gun is an overlay — don't world-shadow it (FPS convention).
    if (gs.players[localID].alive && !connectPrompt.open && !thirdPerson) {
        r.shader.setInt(r.shader.locUseShadow, 0);
        drawViewModel(r, cam, vm, gWeaponId);
        r.shader.setInt(r.shader.locUseShadow, 1);
    }
    if (showHud) {
        gProfiler.beginPass(PASS_HUD);
        drawHUD(r, gs, localID, hud, scoreboard, online, fullMap);
        gProfiler.endPass(PASS_HUD);
    }
    drawConnectPrompt(r, connectPrompt, lobby);
    gProfiler.endFrame();
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

static const char* DEFAULT_SERVER_IP = "127.0.0.1";

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered so logs flush when piped to a file
    platformSocketInit();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    // Offline you sit in the LOBBY (flat test range) until you press C and join a
    // server. setMap must run BEFORE the renderer: the Paldiski heightfield mesh is
    // baked at init (pads must exist), even though the lobby draws a flat quad.
    // FPS_MAP=paldiski forces the real map offline (debug/screenshots).
    setMap(MAP_PALDISKI);                 // generate pads for the terrain-mesh bake
    MapId offlineMap = mapFromName(getenv("FPS_MAP"), MAP_LOBBY);

    Renderer renderer;
    if (!renderer.init("pvp_shooter", 1280, 720)) {
        fprintf(stderr, "renderer init failed\n");
        return 1;
    }
    gProfiler.configureFromEnv();
    applyQuality(renderer, qualityFromEnv());
    SDL_SetRelativeMouseMode(SDL_TRUE);

    Audio audio;
    audio.init();   // runs silent if no device

    // Offline practice state: you + a dummy. setupOffline() (below, after `predicted`
    // exists) switches to `offlineMap` and (re)places both — also used when a server
    // connection drops, returning you to the lobby.
    glm::vec3 spawn0(0.0f), dummyPos(0.0f);
    static GameState offline;             // local practice match vs dummy
    offline.usedMask = 0b11;              // slot 0 = self, slot 1 = dummy
    static GameState display;             // what gets rendered when online

    // Persistent bullet-impact decals on all world surfaces (offline + online).
    static Decal decals[DECAL_MAX];
    int decalCount = 0;       // live decals (<= DECAL_MAX)
    int decalHead  = 0;       // next write index (ring buffer)
    auto addDecal = [&](const glm::vec3& pos, const glm::vec3& normal) {
        decals[decalHead] = {pos, normal};
        decalHead = (decalHead + 1) % DECAL_MAX;
        if (decalCount < DECAL_MAX) decalCount++;
    };

    ClientNet net;
    Player    predicted;                  // own player, client-side predicted
    uint32_t  appliedStateSeq = 0;
    glm::vec3 posError(0.0f);             // smooths the snap to authoritative pos
    const float PRED_SMOOTH_TAU = 0.08f;  // correction half-life (~smoothing window)

    Camera cam;

    // Enter (or re-enter) offline practice on `offlineMap`: place self + dummy on
    // the map's first spawn and face the dummy (lobby: down the firing line).
    auto setupOffline = [&]() {
        setMap(offlineMap);
        spawn0 = gMapSpawnCount > 0 ? gMapSpawns[0] : glm::vec3(0.0f);
        glm::vec3 off = (gMapId == MAP_LOBBY) ? glm::vec3(8.0f, 0.0f, 7.0f)
                                              : glm::vec3(14.0f, 0.0f, 5.0f);
        dummyPos   = spawn0 + off;
        spawn0.y   = terrainHeight(spawn0.x, spawn0.z);
        dummyPos.y = terrainHeight(dummyPos.x, dummyPos.z);
        offline.players[0] = Player{};
        offline.players[1] = Player{};
        offline.players[0].pos = spawn0;
        offline.players[1].pos = dummyPos;
        predicted.pos = spawn0;
        cam.yaw = glm::degrees(atan2f(dummyPos.z - spawn0.z, dummyPos.x - spawn0.x));
    };
    setupOffline();
    if (const char* w = getenv("FPS_WEAPON")) {
        if (strcmp(w, "glock") == 0) {
            gWeaponId = WEP_GLOCK19;
            giveWeapon(offline.players[0], gWeaponId);
        }
    }
    // FPS_YAW/FPS_PITCH=<deg>, FPS_POS=<x,z> override the start view (debug shots).
    if (const char* yv = getenv("FPS_YAW"))   cam.yaw   = (float)atof(yv);
    if (const char* pv = getenv("FPS_PITCH")) cam.pitch = (float)atof(pv);
    if (const char* xv = getenv("FPS_POS")) {
        float px = 0, pz = 0;
        if (sscanf(xv, "%f,%f", &px, &pz) == 2) {
            spawn0 = {px, terrainHeight(px, pz), pz};
            offline.players[0].pos = spawn0;
            predicted.pos = spawn0;
        }
    }
    if (const char* pv = getenv("FPS_PITCH")) cam.pitch = (float)atof(pv);
    const RefCameraPreset* refCam = refCameraFromEnv();
    if (refCam) {
        offlineMap = MAP_PALDISKI;
        setMap(MAP_PALDISKI);
        renderer.setAtmosphere(refCam->atmo);
        applyRefCamera(cam, *refCam);
        glm::vec3 p = {refCam->feet.x,
                       terrainHeight(refCam->feet.x, refCam->feet.z),
                       refCam->feet.z};
        offline.players[0].pos = p;
        predicted.pos = p;
        printf("[ref] camera preset: %s\n", refCam->name);
    }
    FrameInput input;


    // Default: training mode (offline). Only auto-connect when an IP arg is given;
    // otherwise press C to connect to a server.
    if (argc > 1) {
        printf("connecting to %s...\n", argv[1]);
        if (!net.connect(argv[1]))
            printf("connect failed — training mode\n");
    } else {
        printf("training mode — press C to connect to a server\n");
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
    float       stepTimer    = 0.0f;    // footstep cadence
    uint8_t     prevRemoteShots[MAX_PLAYERS] = {0};  // last seen authoritative per-player shot counters
    bool        remoteShotsSeeded = false;           // seed once when entering online play
    int         fireMode     = FIRE_SEMI;
    float       fireTimer    = 0.0f;    // cooldown until next allowed shot
    int         burstRemaining = 0;   // rounds left in current burst
    uint32_t    lastFireEpoch = 0;
    bool        prevReloading = false;  // for reload-start sound
    float       gameTime     = 0.0f;    // accumulated seconds, drives grass wind
    uint8_t     prevOwnHits  = 0;       // hit-marker: own hits-dealt counter
    glm::vec3   hitMarkerPos(0.0f);     // world impact point of the latest hit
    float       recoilHeat   = 0.0f;    // ramps recoil kick over a sustained spray
    float       sinceShot    = 1e9f;    // seconds since last shot (recovery gating)

    bool wasOnline = false;
    ConnectPrompt connectPrompt;
    bool connectPromptActive = false;
    Lobby lobby;   // server browser populated while connectPrompt is in PM_BROWSE

    // Per-remote-player walk animation state (client-side, cosmetic). Phase advances
    // with horizontal speed derived from the interpolated render positions.
    float     walkPhase[MAX_PLAYERS] = {0};
    float     walkAmp[MAX_PLAYERS]   = {0};   // smoothed 0..1 move amount
    float     crouchAnim[MAX_PLAYERS] = {0};  // smoothed 0..1 visible stance
    float     adsAnim[MAX_PLAYERS]   = {0};   // smoothed 0..1 aim-down-sights pose
    float     walkSpeed[MAX_PLAYERS] = {0};   // smoothed horizontal speed (m/s)
    glm::vec3 prevPlayerPos[MAX_PLAYERS];
    bool      prevPosValid[MAX_PLAYERS] = {false};
    Ragdoll   ragdolls[MAX_PLAYERS];          // cosmetic death-flop, seeded on alive→dead
    bool      prevAlive[MAX_PLAYERS];         // last frame's alive flag, for the edge
    for (int i = 0; i < MAX_PLAYERS; i++) prevAlive[i] = true;
    bool      showHitboxes = false;           // H: overlay translucent hit regions
    bool      fullMap      = false;           // M: full-screen map overlay
    bool      showHud      = getenv("FPS_NOHUD") == nullptr;            // J: hide whole HUD for immersion
    bool      thirdPerson  = getenv("FPS_TPP") != nullptr;  // V: orbit self-view (see your own body)

    // Debug: FPS_ATMO=clear|overcast|golden picks the starting atmosphere preset
    // (K cycles at runtime; cosmetic and client-only, so nothing to sync).
    if (const char* at = getenv("FPS_ATMO")) {
        if      (strcmp(at, "overcast") == 0) renderer.setAtmosphere(Renderer::ATMO_OVERCAST);
        else if (strcmp(at, "golden")   == 0) renderer.setAtmosphere(Renderer::ATMO_GOLDEN);
        else                                  renderer.setAtmosphere(Renderer::ATMO_CLEAR);
    }

    auto closeConnectPrompt = [&]() {
        if (!connectPromptActive) return;
        connectPrompt.close();
        lobby.close();
        SDL_StopTextInput();
        SDL_SetRelativeMouseMode(SDL_TRUE);
        connectPromptActive = false;
    };

    printf("controls: WASD move, mouse look, LMB shoot, Q/E lean, 1/2 or scroll weapon "
           "(Uzi/Glock), C connect, V third-person, K atmosphere, F wireframe, H hitboxes, J toggle HUD, ESC quit\n");
    printf("lobby: shooting range + dummy; press C to join a server (Paldiski)\n");

    const char* shotFrameEnv = getenv("FPS_SHOT_FRAME");
    const int shotFrame = shotFrameEnv ? std::max(1, atoi(shotFrameEnv)) : 60;
    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = (float)(now - last) / SDL_GetPerformanceFrequency();
        const float rawFrameMs = dt * 1000.0f;
        last = now;
        if (dt > 0.0f) {              // smoothed FPS from raw frame time (before the cap)
            float ms = rawFrameMs;
            hud.frameMs = hud.frameMs <= 0.0f ? ms : hud.frameMs + (ms - hud.frameMs) * 0.1f;
            hud.fps = 1000.0f / hud.frameMs;
        }
        if (dt > 0.05f) dt = 0.05f;   // cap to avoid spiral

        pollInput(input, cam, &connectPrompt);
        if (input.quit) running = false;
        if (input.wireframeToggle) renderer.toggleWireframe();
        if (input.hitboxToggle) showHitboxes = !showHitboxes;
        if (input.mapToggle) fullMap = !fullMap;
        if (input.hudToggle) showHud = !showHud;
        if (input.thirdPersonToggle) thirdPerson = !thirdPerson;
        if (input.atmoToggle) {
            renderer.setAtmosphere(renderer.atmoPreset + 1);
            static const char* atmoNames[] = {"clear", "overcast", "golden hour"};
            printf("atmosphere: %s\n", atmoNames[renderer.atmoPreset]);
        }
        if (input.clearRange) { decalCount = 0; decalHead = 0; }

        // Weapon select (1 = Uzi, 2 = Glock). Offline re-arms now; online the server
        // adopts it from the input packet and stays authoritative.
        if (input.weaponSelect >= 0 && (uint8_t)input.weaponSelect != gWeaponId) {
            gWeaponId = (uint8_t)input.weaponSelect;
            giveWeapon(offline.players[0], gWeaponId);
        }
        input.state.weaponId = gWeaponId;
        // Repeatable offline asset checks without injecting mouse/keyboard events.
        if (!net.connected) {
            if (getenv("FPS_ADS")) input.state.ads = true;
            if (getenv("FPS_CROUCH")) input.state.crouch = true;
        }

        // Lean (Q/E): smooth toward the held direction; the result drives the camera
        // roll/peek and is sent to the server for the authoritative shot origin.
        float leanTarget = (input.state.leanRight ? 1.0f : 0.0f) -
                           (input.state.leanLeft  ? 1.0f : 0.0f);
        cam.updateLean(leanTarget, dt);
        input.state.lean = cam.lean;
        if (const char* lv = getenv("FPS_LEAN")) { cam.lean = (float)atof(lv); input.state.lean = cam.lean; }

        if (connectPromptActive && !connectPrompt.open) closeConnectPrompt();

        // C opens the modal anytime — even mid-connect or already connected, so you
        // can switch servers without quitting. Game keeps running behind the modal.
        if (input.connectRequested && !connectPrompt.open) {
            connectPrompt.show(DEFAULT_SERVER_IP);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_StartTextInput();
            connectPromptActive = true;
        }

        // PM_IP: Enter starts a lobby scan of the typed host and switches to the
        // server browser. PM_BROWSE: arrows move the highlight, Enter joins the
        // selected map server on its own port.
        if (connectPrompt.open && connectPrompt.mode == PM_IP && input.connectSubmit) {
            const char* ip = connectPrompt.ip[0] ? connectPrompt.ip : DEFAULT_SERVER_IP;
            printf("scanning %s for games...\n", ip);
            lobby.start(ip);
            connectPrompt.mode = PM_BROWSE;
            connectPrompt.sel  = 0;
        } else if (connectPrompt.open && connectPrompt.mode == PM_BROWSE) {
            if (lobby.count > 0) {
                if (input.promptUp)   connectPrompt.sel = (connectPrompt.sel - 1 + lobby.count) % lobby.count;
                if (input.promptDown) connectPrompt.sel = (connectPrompt.sel + 1) % lobby.count;
            }
            if (input.connectSubmit && lobby.count > 0) {
                const ServerEntry& e = lobby.entries[connectPrompt.sel];
                uint16_t port = e.port;
                closeConnectPrompt();
                printf("connecting to %s:%u (%s)...\n", lobby.host, port, e.name);
                if (!net.connect(lobby.host, port))
                    printf("connect failed — offline practice mode\n");
            }
        }

        if (lobby.active) lobby.update(dt);
        net.update(dt);

        bool online  = net.connected && net.hasState;
        int  localID = online ? net.playerID : 0;
        if (localID < 0 || localID >= MAX_PLAYERS) localID = 0;

        if (online && !wasOnline) {   // reset offline→online client state
            // Adopt the map the server advertised in its ACCEPT so client and server
            // always match. Only real maps (< MAP_LOBBY) are valid from a server.
            MapId srv = (net.serverMap >= 0 && net.serverMap < MAP_LOBBY) ? (MapId)net.serverMap
                                                                          : MAP_PALDISKI;
            if (srv != gMapId) setMap(srv);
            prevOwnHP       = PLAYER_HP;
            prevOwnHits     = 0;
            prevOwnAlive    = true;
            appliedStateSeq = 0;
            predicted       = Player{};
            posError        = glm::vec3(0.0f);
            decalCount      = 0;
            decalHead       = 0;
            remoteShotsSeeded = false;
        }
        if (!online && wasOnline) setupOffline();  // back to the lobby test range
        if (!online) remoteShotsSeeded = false;
        wasOnline = online;

        // --- fire control: pick shots this frame by fire mode, gate on ammo ---
        const WeaponDef& lw = weaponDef(gWeaponId);   // local (selected) weapon
        if (lw.semiOnly) fireMode = FIRE_SEMI;        // pistols: semi only
        else if (input.fireModeToggle) {
            fireMode = (fireMode + 1) % FIRE_MODE_COUNT; burstRemaining = 0;
            hud.fireModeTimer = 2.0f;                 // flash the new mode, then fade
        }
        hud.fireMode = fireMode;
        input.state.fireMode = (uint8_t)fireMode;

        bool ownAliveF, ownReloadingF; int ownMagF;   // own weapon state for gating
        if (online) {
            const PlayerNetState& o = net.lastState.players[localID];
            ownAliveF = o.alive != 0; ownMagF = o.mag; ownReloadingF = o.reloading != 0;
        } else {
            const Player& s = offline.players[0];
            ownAliveF = s.alive; ownMagF = s.mag; ownReloadingF = s.reloading;
        }

        if (online && net.lastState.fireEpoch != lastFireEpoch) {
            lastFireEpoch = net.lastState.fireEpoch;
            burstRemaining = 0;
        }
        fireTimer -= dt;
        if (fireMode == FIRE_BURST && input.state.shoot) burstRemaining = lw.burstCount;
        bool  wantFire = false;
        float fireInt  = lw.fireSemiInt;
        if (fireMode == FIRE_SEMI)  { wantFire = input.state.shoot;     fireInt = lw.fireSemiInt;  }
        if (fireMode == FIRE_BURST) { wantFire = burstRemaining > 0;    fireInt = lw.fireBurstInt; }
        if (fireMode == FIRE_AUTO)  { wantFire = input.state.shootHeld; fireInt = lw.fireAutoInt;  }

        bool fireStateReady = !online ||
            (net.lastState.players[localID].weaponId == gWeaponId &&
             net.lastState.fireMode == fireMode && net.lastState.fireEpoch != 0);
        if (!ownAliveF || ownReloadingF || !fireStateReady || input.state.reload)
            burstRemaining = 0;
        bool fired = fireStateReady && !input.state.reload && wantFire && ownAliveF && ownMagF > 0 && !ownReloadingF && fireTimer <= 0.0f;
        if (online) offlineShoot = false;
        if (fired) {
            fireTimer = fireInt;
            if (fireMode == FIRE_BURST && burstRemaining > 0) burstRemaining--;
            if (online) net.shotSeq++;   // request; server validates cadence and decrements ammo on spawn
            else        offlineShoot = true;
            vm.flashTimer = 0.05f;
            vm.recoilT    = 1.0f;
            audioPlay(weaponShootSound(gWeaponId));
        }
        if (input.state.shoot && ownAliveF && ownMagF == 0 && !ownReloadingF) audioPlay(SND_DRYFIRE);
        if (ownReloadingF && !prevReloading) audioPlay(SND_RELOAD);
        prevReloading = ownReloadingF;

        // Report input (incl. this frame's shotSeq) with the pre-kick aim, so the
        // shot leaves the barrel exactly where aimed; the kick lands afterwards.
        if (net.connected) {
            uint32_t viewSeq  = 0;
            uint8_t  viewFrac = 0;
            if (net.hasState) {
                float ps = net.playSeq < 0.0f ? 0.0f : net.playSeq;  // playout pos = render time
                viewSeq  = (uint32_t)floorf(ps);
                viewFrac = (uint8_t)((ps - (float)viewSeq) * 255.0f + 0.5f);
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
            float ramp = glm::min(t, 1.0f);
            float vKick = glm::mix(RECOIL_PITCH_MIN, RECOIL_PITCH_MAX, ramp) * mult;
            if (t > 1.0f) vKick += (t - 1.0f) * RECOIL_HEAT_OVER * mult;
            float hKick = RECOIL_YAW * (0.6f + 0.8f * ramp) * mult * rs;
            if (t > 1.0f) hKick *= 1.0f + (t - 1.0f) * 0.05f;
            cam.applyRecoil(vKick, hKick);
            recoilHeat += 1.0f;
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
                    weaponShot(input.state.yaw, input.state.pitch, cam.eyePos(), input.state.ads,
                               spread, origin, dir);
                    spawnBullet(offline, origin, dir, 0);
                    offlineShoot = false;
                }
                movePlayer(self, input.state, FIXED_DT);   // wall push-out via collideXZ
                updateReload(self, input.state.reload, FIXED_DT);
                if (self.mag == 0 && self.reserve == 0)                       // keep practice stocked
                    self.reserve = weaponDef(self.weaponId).reservePerLife;
                // updateBullets stops rounds on any surface and reports the impacts
                // (pos + face normal); stamp a decal on each, exactly as the server does
                // online. Wall, cover, ground — every surface marks the same way.
                Impact imp[NET_MAX_IMPACTS];
                int impCount = 0;
                updateBullets(offline, FIXED_DT, nullptr, nullptr, imp, &impCount, NET_MAX_IMPACTS);
                for (int k = 0; k < impCount; k++) addDecal(imp[k].pos, imp[k].normal);
                if (!dummy.alive) {                                 // offline dummy respawn
                    dummy.respawnTimer -= FIXED_DT;
                    if (dummy.respawnTimer <= 0.0f) {
                        dummy = Player{};
                        dummy.pos = dummyPos;
                    }
                }
            }
            accumulator -= FIXED_DT;
        }

        // movePlayer only sets yaw + crouch, so reflect aim pitch, lean, and ADS onto
        // the offline avatar too (kept for future self-views; harmless otherwise).
        offline.players[0].pitch = input.state.pitch;
        offline.players[0].lean  = input.state.lean;
        offline.players[0].ads   = input.state.ads;

        // Lobby dummy mimics your pose: aim, lean, stance, and gun — but pinned to
        // its yard with a fixed facing (so you can study it from any angle), walking
        // only animates it in place (walk-anim slot mirrored below) and jumps replay
        // as hops on the spot.
        if (!online && offline.players[1].alive) {
            const Player& self  = offline.players[0];
            Player&       dummy = offline.players[1];
            dummy.pitch    = self.pitch;
            dummy.lean     = self.lean;
            dummy.ads      = self.ads;
            dummy.crouched = self.crouched;
            dummy.weaponId = self.weaponId;
            float air = self.pos.y - terrainHeight(self.pos.x, self.pos.z);
            dummy.pos = dummyPos + glm::vec3(0.0f, air > 0.0f ? air : 0.0f, 0.0f);
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
            StatePacket sa, sb;
            float alpha = 0.0f;
            net.sampleRender(sa, sb, alpha);   // playout-buffered: smooth under jitter/loss
            unpackState(sa, sb, alpha, display);
            display.players[localID].pos = predicted.pos + posError;  // smoothed own view
            shown = &display;
        }
        posError *= expf(-dt / PRED_SMOOTH_TAU);
        if (glm::length(posError) < 0.0005f) posError = glm::vec3(0.0f);

        // Drain server-sent world impacts into decals (online stamping path).
        for (int k = 0; k < net.impactQCount; k++) {
            const ImpactNetState& im = net.impactQ[k];
            addDecal({im.x, im.y, im.z}, IMPACT_DIRS[im.dir < 6 ? im.dir : IMP_PY]);
        }
        net.impactQCount = 0;

        // Stance is authoritative immediately, but human bodies do not change height
        // in one frame. Ease both the first-person eye and every rendered skeleton.
        float crouchK = dt * CROUCH_LERP_SPEED;
        if (crouchK > 1.0f) crouchK = 1.0f;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            bool visible = (shown->usedMask & (1u << i)) && shown->players[i].alive;
            if (!visible) { crouchAnim[i] = 0.0f; continue; }
            float target = shown->players[i].crouched ? 1.0f : 0.0f;
            crouchAnim[i] += (target - crouchAnim[i]) * crouchK;
        }
        float eyeH = glm::mix(EYE_HEIGHT, CROUCH_EYE, crouchAnim[localID]);
        cam.eye = shown->players[localID].pos + glm::vec3(0, eyeH, 0);

        const Player& own = shown->players[localID];
        if (own.hp < prevOwnHP && own.alive) hud.flashTimer = 0.4f;  // got hit
        prevOwnHP = own.hp;
        hud.flashTimer -= dt;
        if (hud.flashTimer < 0.0f) hud.flashTimer = 0.0f;
        hud.fireModeTimer -= dt;
        if (hud.fireModeTimer < 0.0f) hud.fireModeTimer = 0.0f;

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

        // footsteps while moving on the ground. Airborne test = feet above the
        // support height (terrain or box top under the footprint), mirroring
        // physics supportHeight(). Frame-to-frame dY can't be used: walking up/down
        // any sloped terrain changes Y every frame, which made onGround flicker and
        // spam the "instant first step" reset (rapid-step bug on uneven ground).
        float support = terrainHeight(own.pos.x, own.pos.z);
        for (int i = 0; i < gMapBoxCount; i++) {
            const Box& b = gMapBoxes[i];
            if (fabsf(own.pos.x - b.center.x) >= b.half.x + 0.4f) continue;
            if (fabsf(own.pos.z - b.center.z) >= b.half.z + 0.4f) continue;
            float top = b.center.y + b.half.y;
            if (top > support && own.pos.y >= top - 0.05f) support = top;
        }
        bool  onGround = (own.pos.y - support) < 0.12f;   // not jumping/falling
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

        // Enemy gunshots: use per-player authoritative shot counters (from server).
        // This is robust under packet loss/jitter and independent from bullet replication.
        if (online) {
            const StatePacket& st = net.lastState;
            if (!remoteShotsSeeded) {
                for (int i = 0; i < MAX_PLAYERS; i++)
                    prevRemoteShots[i] = st.players[i].shotsFired;
                remoteShotsSeeded = true;
            }
            for (int i = 0; i < MAX_PLAYERS; i++) {
                uint8_t cur = st.players[i].shotsFired;
                uint8_t delta = (uint8_t)(cur - prevRemoteShots[i]);  // wrap-safe
                prevRemoteShots[i] = cur;
                if (i == localID) continue;
                if (!(st.usedMask & (1u << i))) continue;
                if (delta == 0 || delta >= 128) continue;  // ignore resets / stale jumps

                int plays = delta <= 4 ? (int)delta : 4;  // cap catch-up bursts
                SoundId snd = weaponShootSound(st.players[i].weaponId);
                for (int n = 0; n < plays; n++)
                    audioPlayAt(snd, shown->players[i].pos, cam.eye, cam.front());
            }
        }

        // Cosmetic ragdolls: seed on the alive→dead edge (carrying the corpse's last
        // velocity so a runner tumbles), clear on respawn/leave, and step every frame.
        // prevPlayerPos still holds last frame here (the walk loop below refreshes it).
        for (int i = 0; i < MAX_PLAYERS; i++) {
            bool used  = shown->usedMask & (1u << i);
            bool alive = used && shown->players[i].alive;
            if (used && prevAlive[i] && !alive) {
                glm::vec3 vel(0.0f);
                if (prevPosValid[i])
                    vel = (shown->players[i].pos - prevPlayerPos[i]) / (dt > 1e-4f ? dt : 1e-4f);
                ragdolls[i].seed(shown->players[i].pos, shown->players[i].yaw, COLOR_ENEMY, vel);
            }
            if (alive || !used) ragdolls[i].active = false;
            ragdolls[i].step(dt);
            prevAlive[i] = alive;
        }

        // Advance each visible player's walk animation from how fast their rendered
        // position moves on the ground. Includes the local player so the training
        // mirror's self-reflection animates while you walk.
        for (int i = 0; i < MAX_PLAYERS; i++) {
            bool show = (shown->usedMask & (1u << i)) && shown->players[i].alive;
            if (!show) { walkAmp[i] = 0.0f; adsAnim[i] = 0.0f; prevPosValid[i] = false; continue; }
            // Smooth the authoritative 0/1 ADS toward the held pose at the same rate as
            // the first-person ADS transition, so the raised gun eases in/out.
            float adsK = dt * ADS_LERP_SPEED; if (adsK > 1.0f) adsK = 1.0f;
            adsAnim[i] += ((shown->players[i].ads ? 1.0f : 0.0f) - adsAnim[i]) * adsK;
            glm::vec3 pp = shown->players[i].pos;
            float sp = 0.0f;
            if (prevPosValid[i]) {
                glm::vec3 d = pp - prevPlayerPos[i]; d.y = 0.0f;
                sp = glm::length(d) / (dt > 1e-4f ? dt : 1e-4f);
            }
            prevPlayerPos[i] = pp; prevPosValid[i] = true;
            walkSpeed[i] += (sp - walkSpeed[i]) * 0.30f;            // smooth the jitter
            float amp = walkSpeed[i] / MOVE_SPEED;
            walkAmp[i] = amp > 1.0f ? 1.0f : amp;
            walkPhase[i] += walkSpeed[i] * 1.8f * dt;               // stride tied to speed
        }
        // The pinned lobby dummy never moves, so hand it your stride instead —
        // it walks on the spot whenever you walk.
        if (!online) { walkAmp[1] = walkAmp[0]; walkPhase[1] = walkPhase[0]; }

        gameTime += dt;
        renderer.setTime(gameTime);
        hud.adsT = vm.adsT;
        // Third-person boom with collision: march back from the head along -front, stop
        // short of terrain or any cover box so the camera never clips through the world
        // (uncollided it dives underground when you aim up). A small up-tilt over the
        // shoulder; the resolved point is handed to the camera for view().
        cam.tpDist = thirdPerson ? 1.0f : 0.0f;
        if (thirdPerson) {
            glm::vec3 head = cam.eyePos() + glm::vec3(0.0f, 0.25f, 0.0f);
            glm::vec3 back = -cam.front();
            float maxD = 3.0f, d = maxD;
            for (float t = 0.15f; t <= maxD; t += 0.15f) {
                glm::vec3 c = head + back * t;
                bool hit = c.y < terrainHeight(c.x, c.z) + 0.2f;
                for (int bi = 0; bi < gMapBoxCount && !hit; bi++) {
                    const Box& b = gMapBoxes[bi];
                    glm::vec3 e2 = b.half + glm::vec3(0.2f);   // pad so we stop before the face
                    if (fabsf(c.x - b.center.x) < e2.x && fabsf(c.y - b.center.y) < e2.y &&
                        fabsf(c.z - b.center.z) < e2.z) hit = true;
                }
                if (hit) { d = t - 0.15f; break; }
            }
            if (d < 0.4f) d = 0.4f;                            // don't jam inside the body
            glm::vec3 cpos = head + back * d;
            float gy = terrainHeight(cpos.x, cpos.z) + 0.2f;
            if (cpos.y < gy) cpos.y = gy;
            cam.tpPos = cpos;
        }
        // Automated reference runs must render the same view even if desktop
        // mouse/focus events arrive while the benchmark or capture is running.
        if (refCam && (getenv("FPS_BENCH") || getenv("FPS_SHOT"))) {
            applyRefCamera(cam, *refCam);
            cam.tpDist = 0.0f;
        }
        Uint64 renderStart = SDL_GetPerformanceCounter();
        renderScene(renderer, cam, *shown, localID, hud, input.scoreboardHeld, online, vm,
                    decals, decalCount, connectPrompt, lobby, walkPhase, walkAmp,
                    crouchAnim, adsAnim, showHitboxes, fullMap, showHud, thirdPerson, ragdolls);
        float renderMs = (float)(SDL_GetPerformanceCounter() - renderStart) * 1000.0f /
                         (float)SDL_GetPerformanceFrequency();
        hud.renderCpuMs = hud.renderCpuMs <= 0.0f ? renderMs
                          : hud.renderCpuMs + (renderMs - hud.renderCpuMs) * 0.1f;

        static int frameCount = 0;
        ++frameCount;
        const char* shotPath = getenv("FPS_SHOT");
        if (shotPath && frameCount == shotFrame) {
            printf("[shot] weapon=%u ads=%.3f crouch=%.3f\n", (unsigned)gWeaponId,
                   vm.adsT, crouchAnim[localID]);
            dumpFrame(renderer, shotPath);
            running = false;
        }
        // Debug: FPS_BENCH=1 prints steady-state frame stats and quits. Skips the
        // first 300 frames (shader compile, tree scatter, impostor bake pollute
        // the HUD's smoothed readout), then samples 600 raw frame times.
        if (getenv("FPS_BENCH")) {
            static float benchMs[600];   // pre-allocated: no heap in the loop
            static float benchPass[PASS_COUNT][600];
            static int   benchN = 0;
            if (frameCount == 301) {
                for (int p = 0; p < PASS_COUNT; p++) gProfiler.passMs[p] = 0.0f;
            }
            if (frameCount > 300 && benchN < 600) {
                benchMs[benchN] = rawFrameMs;  // include hitches beyond the simulation cap
                for (int p = 0; p < PASS_COUNT; p++)
                    benchPass[p][benchN] = gProfiler.passMsRaw[p];
                benchN++;
            }
            if (benchN == 600) {
                std::sort(benchMs, benchMs + 600);
                float avg = 0.0f;
                for (float m : benchMs) avg += m;
                printf("[bench] quality=%s  frame ms avg %.2f  p50 %.2f  p95 %.2f  max %.2f\n",
                       gQuality.name, avg / 600.0f, benchMs[300], benchMs[570], benchMs[599]);
                printf("[bench] render passes (CPU submission/wait, avg ms over sample):");
                for (int p = 0; p < PASS_COUNT; p++) {
                    float pavg = 0.0f;
                    for (int i = 0; i < 600; i++) pavg += benchPass[p][i];
                    printf(" %s=%.2f", FrameProfiler::passName((RenderPass)p), pavg / 600.0f);
                }
                printf("\n[bench] veg instances: L0=%d L1=%d imp=%d bush=%d\n",
                       gVegStats.treesL0, gVegStats.treesL1,
                       gVegStats.treesImp, gVegStats.bushes);
                running = false;
            }
        }
    }

    closeConnectPrompt();
    net.disconnect();
    audio.shutdown();
    renderer.shutdown();
    SDL_Quit();
    platformSocketCleanup();
    return 0;
}
