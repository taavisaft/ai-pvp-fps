// Client: starts in training mode (offline) by default. Auto-connects only with
// `./game <ip>`. Press C for an in-game IP prompt (127.0.0.1 pre-filled).
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#include "material.h"
#include "connect_prompt.h"
#include "lobby.h"
#include "skeleton.h"
#include "ragdoll.h"
#include "weapon_visual.h"
#include "playerpose.h"

static const glm::vec3 COLOR_ENEMY        = {0.80f, 0.30f, 0.20f};
static const glm::vec3 COLOR_BULLET_OWN   = {1.00f, 0.90f, 0.20f};
static const glm::vec3 COLOR_BULLET_ENEMY = {1.00f, 0.40f, 0.10f};

// Persistent bullet-impact decal: a small patch laid flat on whatever surface the round
// struck (ground, cover, walls). Stamped from the local sim offline and from the
// server's PKT_IMPACT online, so every world detail shows hits, not just the range wall.
struct Decal { glm::vec3 pos; glm::vec3 normal; };
static const glm::vec3 COLOR_DECAL = {1.0f, 0.85f, 0.20f};   // matches the old range-wall marks
// Axis-aligned normals, indexed by the protocol's ImpactDir code (IMP_PX..IMP_NZ).
static const glm::vec3 IMPACT_DIRS[6] = {
    {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1},
};

constexpr int DECAL_MAX = 512;    // ring buffer of impact decals

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
    glm::vec3 up    = cam.up();                                   // rolled by lean
    glm::vec3 right = glm::normalize(glm::cross(front, up));

    glm::vec3 hipOff = right * 0.17f - up * 0.30f + front * 0.35f;
    // Uzi: align hollow sight center (local y=0.06, z=-0.02) with the aim ray.
    glm::vec3 adsOff = (gWeaponId == WEP_UZI)
        ? (-up * 0.06f + front * 0.32f)
        : (-up * 0.05f + front * 0.30f);
    glm::vec3 off    = glm::mix(hipOff, adsOff, vm.adsT);
    glm::vec3 anchor = cam.eyePos() + off - front * (vm.recoilT * 0.08f);

    glm::mat4 basis(glm::vec4(right, 0), glm::vec4(up, 0),
                    glm::vec4(front, 0), glm::vec4(0, 0, 0, 1));
    glm::mat4 anchorM = glm::translate(glm::mat4(1.0f), anchor) * basis;
    auto part = [&](glm::vec3 lp, glm::vec3 sz, glm::vec3 col) {
        glm::mat4 m = glm::scale(glm::translate(anchorM, lp), sz);
        r.drawCubeModel(m, col);
    };
    const glm::vec3 metal = {0.12f, 0.12f, 0.14f};
    const glm::vec3 dark  = {0.20f, 0.20f, 0.23f};
    const glm::vec3 poly  = {0.08f, 0.08f, 0.09f};  // pistol polymer frame

    if (gWeaponId == WEP_GLOCK19) {                 // compact pistol
        part({0.0f,  0.02f,  0.06f}, {0.050f, 0.060f, 0.20f}, metal); // slide
        part({0.0f, -0.03f,  0.03f}, {0.045f, 0.050f, 0.15f}, poly);  // frame
        part({0.0f,  0.02f,  0.17f}, {0.026f, 0.026f, 0.05f}, dark);  // barrel tip
        part({0.0f, -0.12f, -0.05f}, {0.050f, 0.130f, 0.06f}, poly);  // grip
        part({0.0f, -0.18f, -0.05f}, {0.046f, 0.040f, 0.05f}, metal); // mag base
    } else {                                        // Uzi — boxy SMG
        part({0.0f,  0.00f,  0.01f}, {0.078f, 0.10f,  0.26f}, metal); // receiver
        // Red-dot sight — hollow housing (see-through window along barrel axis).
        {
            const glm::vec3 sc = {0.0f, 0.06f, -0.02f};
            const float wx = 0.050f, wy = 0.040f, wz = 0.12f, wt = 0.009f;
            part({sc.x, sc.y + wy * 0.5f - wt * 0.5f, sc.z}, {wx, wt, wz}, dark); // top
            part({sc.x, sc.y - wy * 0.5f + wt * 0.5f, sc.z}, {wx, wt, wz}, dark); // bottom
            part({sc.x - wx * 0.5f + wt * 0.5f, sc.y, sc.z}, {wt, wy - wt * 2.0f, wz}, dark); // left
            part({sc.x + wx * 0.5f - wt * 0.5f, sc.y, sc.z}, {wt, wy - wt * 2.0f, wz}, dark); // right
        }
        part({0.0f,  0.02f,  0.22f}, {0.032f, 0.032f, 0.16f}, dark);  // barrel
        part({0.0f, -0.11f, -0.02f}, {0.050f, 0.140f, 0.07f}, metal); // grip
        part({0.0f, -0.20f, -0.02f}, {0.044f, 0.190f, 0.05f}, dark);  // long magazine
    }

    // Grip + muzzle come from the shared per-weapon anchor table (weapon_visual.h), so
    // every gun's hand placement and barrel length live in one place.
    const WeaponVisual& wv = weaponVisual(gWeaponId);
    const float gripY = wv.fpFireGrip.y, gripZ = wv.fpFireGrip.z;

    // Right (firing) hand only — PUBG-style, minimal screen space. A fist wrapping
    // the grip plus a short forearm angled down-back so it leaves frame fast; the gun
    // sits in front and occludes most of it. ADS/recoil come free from anchor space.
    const glm::vec3 glove = {0.46f, 0.35f, 0.28f};   // tan fingerless glove
    glm::vec3 fistC = {wv.fpFireGrip.x, gripY, gripZ};  // fist centered on the grip
    r.drawCubeModel(glm::scale(glm::translate(anchorM, fistC),
                               glm::vec3(0.085f, 0.095f, 0.105f)), glove);
    // Forearm: pivot at the wrist (just under the fist), tilt back about the right
    // axis so the limb runs from the lower-right toward the shoulder, then hang it.
    glm::mat4 fore = glm::translate(anchorM, glm::vec3(0.02f, gripY - 0.05f, gripZ - 0.01f))
                   * glm::rotate(glm::mat4(1.0f), glm::radians(24.0f), glm::vec3(1, 0, 0))
                   * glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0, 0, 1))
                   * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.11f, 0.0f));
    r.drawCubeModel(glm::scale(fore, glm::vec3(0.072f, 0.22f, 0.082f)), glove);

    if (vm.flashTimer > 0.0f) {                                    // muzzle flash
        glm::vec3 muzzle = anchor + up * wv.fpMuzzle.y + front * wv.fpMuzzle.z;
        r.drawCube(muzzle, glm::vec3(0.16f), {1.0f, 0.85f, 0.35f});
    }
}

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
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(bz * 0.14f, 0.0f);
    m[1] = glm::vec4(n  * 0.02f, 0.0f);
    m[2] = glm::vec4(bx * 0.14f, 0.0f);
    m[3] = glm::vec4(d.pos + n * 0.02f, 1.0f);    // sit proud of the surface (no z-fight)
    r.drawCubeModel(m, COLOR_DECAL);
}

// Renders a player from the shared pose builder (playerpose.h): FK spine, IK arms
// onto the weapon grips, the weapon, IK legs — the very same oriented boxes the server
// hit-tests, just colored by part here. Aim pitch tilts the head + gun, lean rolls the
// upper body, crouch squashes vertically, feet plant on the terrain with a
// speed-scaled walk stride.
static void drawPlayerSkeleton(Renderer& r, const glm::vec3& pos, float yaw, float pitch,
                               float lean, uint8_t weaponId, bool crouched, float ads,
                               float phase, float amp, const glm::vec3& bodyCol) {
    const glm::vec3 limb     = bodyCol * 0.85f;
    const glm::vec3 skin     = {0.90f, 0.78f, 0.62f};
    const glm::vec3 handCol  = {0.30f, 0.30f, 0.33f};
    const glm::vec3 gunMetal = {0.12f, 0.12f, 0.14f};
    const glm::vec3 gunDark  = {0.20f, 0.20f, 0.23f};
    const glm::vec3 nose     = {0.90f, 0.30f, 0.20f};

    PoseBox boxes[MAX_POSE_BOXES];
    int n = buildPlayerPose(pos, yaw, pitch, lean, weaponId, crouched, ads, phase, amp,
                            boxes);
    for (int i = 0; i < n; i++) {
        glm::vec3 c;
        switch (boxes[i].part) {
            case POSE_HEAD:      c = skin;     break;
            case POSE_NOSE:      c = nose;     break;
            case POSE_ARM: case POSE_LEG: case POSE_FOOT: c = limb; break;
            case POSE_HAND:      c = handCol;  break;
            case POSE_GUN_METAL: c = gunMetal; break;
            case POSE_GUN_DARK:  c = gunDark;  break;
            default:             c = bodyCol;  break;   // pelvis/torso/neck
        }
        r.drawCubeModel(boxes[i].M * glm::scale(glm::mat4(1.0f), boxes[i].half * 2.0f), c);
    }
}

// Shadow casters + receivers: ground/terrain, cover boxes, remote players, bullets.
// Drawn twice per frame — once into the sun's depth map, once into the main view —
// so it must contain only real world geometry (no HUD, viewmodel, mirror, or the
// cosmetic ground blobs / aim cross, which are added in the main pass only).
static void drawWorldGeometry(Renderer& r, const GameState& gs, int localID,
                              const float* walkPhase, const float* walkAmp,
                              const float* adsAnim, bool showLocal,
                              const Ragdoll* ragdolls,
                              const Frustum& fr, const glm::vec3& eye) {
    if (gMapId == MAP_LOBBY) {
        // Flat pad + plain material boxes (target wall, test cover).
        r.drawTerrain(fr, eye);
        for (int i = 0; i < gMapBoxCount; i++) {
            const Box& b = gMapBoxes[i];
            if (!fr.aabbVisible(b.center, b.half)) continue;
            r.drawCube(b.center, b.half * 2.0f, mapBoxMaterial(i));
        }
    } else {
        r.drawTerrain(fr, eye);
        r.drawGrass(eye);
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == localID && !showLocal) continue;   // draw self in third-person test view
        if (!(gs.usedMask & (1u << i))) continue;
        if (!gs.players[i].alive) {
            if (ragdolls && ragdolls[i].active) ragdolls[i].draw(r);  // cosmetic corpse
            continue;
        }
        const Player& pl = gs.players[i];
        drawPlayerSkeleton(r, pl.pos, pl.yaw, pl.pitch, pl.lean, pl.weaponId, pl.crouched,
                           adsAnim[i], walkPhase[i], walkAmp[i], COLOR_ENEMY);
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
                        const float* adsAnim, bool showHitboxes, bool fullMap, bool showHud,
                        bool thirdPerson, const Ragdoll* ragdolls) {
    static const glm::vec3 COLOR_BLOB = {0.16f, 0.27f, 0.16f};  // ground, darkened

    r.invalidateWorldOnMapChange();   // drop town/tree/prop caches on map switch

    // Pass 1: scene depth from the sun, focused on the camera (near-field shadows).
    r.beginShadowPass(cam.eye);
    Frustum sunFr = Frustum::fromVP(r.lightSpace);
    drawWorldGeometry(r, gs, localID, walkPhase, walkAmp, adsAnim, thirdPerson, ragdolls, sunFr, cam.eye);
    r.endShadowPass();

    // Pass 2: lit main view, sampling the shadow map built above.
    r.beginFrame(cam.view(), cam.proj(r.aspect()), cam.eye);
    r.drawSky(cam.view(), cam.proj(r.aspect()));
    Frustum camFr = Frustum::fromVP(cam.proj(r.aspect()) * cam.view());
    drawWorldGeometry(r, gs, localID, walkPhase, walkAmp, adsAnim, thirdPerson, ragdolls, camFr, cam.eye);
    r.drawWater();   // translucent Baltic, after all opaque world geometry

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
        drawViewModel(r, cam, vm);
        r.shader.setInt(r.shader.locUseShadow, 1);
    }
    if (showHud) drawHUD(r, gs, localID, hud, scoreboard, online, fullMap);
    drawConnectPrompt(r, connectPrompt, lobby);
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
    float       fpsAvg       = 0.0f;    // smoothed FPS readout
    int         fireMode     = FIRE_SEMI;
    float       fireTimer    = 0.0f;    // cooldown until next allowed shot
    int         burstRemaining = 0;     // rounds left in current burst
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
    float     adsAnim[MAX_PLAYERS]   = {0};   // smoothed 0..1 aim-down-sights pose
    float     walkSpeed[MAX_PLAYERS] = {0};   // smoothed horizontal speed (m/s)
    glm::vec3 prevPlayerPos[MAX_PLAYERS];
    bool      prevPosValid[MAX_PLAYERS] = {false};
    Ragdoll   ragdolls[MAX_PLAYERS];          // cosmetic death-flop, seeded on alive→dead
    bool      prevAlive[MAX_PLAYERS];         // last frame's alive flag, for the edge
    for (int i = 0; i < MAX_PLAYERS; i++) prevAlive[i] = true;
    bool      showHitboxes = false;           // H: overlay translucent hit regions
    bool      fullMap      = false;           // M: full-screen map overlay
    bool      showHud      = true;            // J: hide whole HUD for immersion
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

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = (float)(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (dt > 0.0f) {              // smoothed FPS from raw frame time (before the cap)
            float inst = 1.0f / dt;
            fpsAvg = fpsAvg <= 0.0f ? inst : fpsAvg + (inst - fpsAvg) * 0.1f;
            hud.fps = fpsAvg;
            float ms = dt * 1000.0f;
            hud.frameMs = hud.frameMs <= 0.0f ? ms : hud.frameMs + (ms - hud.frameMs) * 0.1f;
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

        bool ownAliveF, ownReloadingF; int ownMagF;   // own weapon state for gating
        if (online) {
            const PlayerNetState& o = net.lastState.players[localID];
            ownAliveF = o.alive != 0; ownMagF = o.mag; ownReloadingF = o.reloading != 0;
        } else {
            const Player& s = offline.players[0];
            ownAliveF = s.alive; ownMagF = s.mag; ownReloadingF = s.reloading;
        }

        fireTimer -= dt;
        if (fireMode == FIRE_BURST && input.state.shoot) burstRemaining = lw.burstCount;
        bool  wantFire = false;
        float fireInt  = lw.fireSemiInt;
        if (fireMode == FIRE_SEMI)  { wantFire = input.state.shoot;     fireInt = lw.fireSemiInt;  }
        if (fireMode == FIRE_BURST) { wantFire = burstRemaining > 0;    fireInt = lw.fireBurstInt; }
        if (fireMode == FIRE_AUTO)  { wantFire = input.state.shootHeld; fireInt = lw.fireAutoInt;  }

        bool fired = wantFire && ownAliveF && ownMagF > 0 && !ownReloadingF && fireTimer <= 0.0f;
        if (online) offlineShoot = false;
        if (fired) {
            fireTimer = fireInt;
            if (fireMode == FIRE_BURST && burstRemaining > 0) burstRemaining--;
            if (online) net.shotSeq++;   // reliable shot; server spawns + decrements mag
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

        float eyeH = shown->players[localID].crouched ? CROUCH_EYE : EYE_HEIGHT;
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
        Uint64 renderStart = SDL_GetPerformanceCounter();
        renderScene(renderer, cam, *shown, localID, hud, input.scoreboardHeld, online, vm,
                    decals, decalCount, connectPrompt, lobby, walkPhase, walkAmp,
                    adsAnim, showHitboxes, fullMap, showHud, thirdPerson, ragdolls);
        float renderMs = (float)(SDL_GetPerformanceCounter() - renderStart) * 1000.0f /
                         (float)SDL_GetPerformanceFrequency();
        hud.renderCpuMs = hud.renderCpuMs <= 0.0f ? renderMs
                          : hud.renderCpuMs + (renderMs - hud.renderCpuMs) * 0.1f;

        static int frameCount = 0;
        const char* shotPath = getenv("FPS_SHOT");
        if (shotPath && ++frameCount == 60) { dumpFrame(renderer, shotPath); running = false; }
    }

    closeConnectPrompt();
    net.disconnect();
    audio.shutdown();
    renderer.shutdown();
    SDL_Quit();
    platformSocketCleanup();
    return 0;
}
