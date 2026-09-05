#include "server_fire.h"
#include "physics.h"
#include "map.h"
#include "server_rewind.h"
#include <cmath>
#include <cstdio>
#include <limits>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

struct Fixture {
    GameState gs{};
    ServerFire fire;
    uint32_t epochs = 0;
    InputPacket packet{};
    Fixture(uint8_t weapon = WEP_UZI, uint8_t mode = FIRE_SEMI) {
        gs.usedMask = 1;
        gs.players[0].pos = {0, 0, 0};
        giveWeapon(gs.players[0], weapon);
        fire.reset(gs.players[0], epochs);
        fire.synchronize(gs.players[0], mode, epochs);
        packet.type = PKT_INPUT;
        packet.weaponId = weapon;
        packet.fireMode = mode;
        packet.fireEpoch = fire.epoch;
        packet.flags = FLAG_ADS;
    }
    void send(uint32_t seq, double now = 0) {
        packet.shotSeq = seq;
        fire.receive(packet, gs.players[0], now);
    }
    bool tick(double now) { return fire.tick(gs, 0, now); }
    void sync() {
        fire.synchronize(gs.players[0], packet.fireMode, epochs);
    }
};

static void cadence() {
    for (uint8_t weapon = 0; weapon < WEP_COUNT; ++weapon) {
        for (uint8_t mode = 0; mode < FIRE_MODE_COUNT; ++mode) {
            if (weaponDef(weapon).semiOnly && mode != FIRE_SEMI) continue;
            Fixture f(weapon, mode);
            const auto& wd = weaponDef(weapon);
            double interval = mode == FIRE_AUTO ? wd.fireAutoInt :
                              mode == FIRE_BURST ? wd.fireBurstInt : wd.fireSemiInt;
            f.send(3);  // three shots recovered from a lost packet, never compressed
            double last = -1;
            int fired = 0;
            for (int tick = 0; tick <= 21; ++tick) {
                double now = tick / 60.0;
                if (!f.tick(now)) continue;
                if (last >= 0) CHECK(now - last + 1e-8 >= interval);
                last = now;
                ++fired;
            }
            CHECK(fired == 3);
            CHECK(f.gs.players[0].mag == wd.magSize - 3);
            CHECK(f.fire.shotsFired == 3);
        }
    }
    Fixture spam;
    double last = -1;
    for (int tick = 0; tick < 120; ++tick) {
        double now = tick / 60.0;
        spam.send(tick + 1, now);
        if (spam.tick(now)) {
            if (last >= 0) CHECK(now - last + 1e-8 >= UZI.fireSemiInt);
            last = now;
        }
        CHECK(spam.fire.pendingCount() <= ServerFire::MAX_PENDING);
    }
    CHECK(spam.fire.shotsFired <= 17); // hostile request stream cannot exceed semi rate
}

static void queueAndSerials() {
    Fixture f;
    f.send(20);  // regression: old server fired all 20 in <450 ms
    CHECK(f.fire.pendingCount() == 0);
    CHECK(!f.tick(0));
    CHECK(f.gs.players[0].mag == UZI.magSize);
    f.send(21);
    f.send(21); // duplicate
    f.send(20); // backwards
    CHECK(f.fire.pendingCount() == 1);
    CHECK(f.tick(0));
    CHECK(!f.tick(1));
    f.send(25, 2); // bounded batch of four
    f.send(26, 2); // queue full: consume/reject, don't retry it later
    CHECK(f.fire.pendingCount() == 4);
    CHECK(!f.tick(2.36)); // no shots emitted after expiry
    f.send(26, 3);
    CHECK(!f.tick(3));
    f.send(27, 3);
    CHECK(f.tick(3)); // rejected batch did not create permanent catch-up debt

    Fixture wrap;
    wrap.send(0x7fffffffu); // oversized, but establishes the high-water mark
    wrap.send(0xfffffffeu);
    wrap.send(0xffffffffu);
    wrap.send(0u);
    CHECK(wrap.fire.pendingCount() == 2);
    CHECK(wrap.tick(0));
    CHECK(wrap.tick(.14));
    CHECK(serialNewer(0u, 0xffffffffu));
    CHECK(!serialNewer(0xffffffffu, 0u));
    CHECK(!serialNewer(0x80000000u, 0u));
}

static void stateChanges() {
    Fixture f;
    f.send(3);
    CHECK(f.tick(0));
    uint32_t old = f.fire.epoch;
    giveWeapon(f.gs.players[0], WEP_GLOCK19);
    f.sync();
    CHECK(f.fire.epoch != old && f.fire.pendingCount() == 0);
    f.packet.weaponId = WEP_GLOCK19;
    f.send(4); // packet already in flight during swap, old epoch
    CHECK(!f.tick(.2));
    f.packet.fireEpoch = f.fire.epoch;
    f.send(1, .05);
    CHECK(!f.tick(.05)); // swap must preserve the previous weapon's cooldown
    CHECK(f.tick(.14));
    CHECK(f.gs.players[0].mag == GLOCK19.magSize - 1);
    CHECK(f.gs.players[0].magW[WEP_UZI] == UZI.magSize - 1);

    Fixture mode;
    mode.send(2);
    CHECK(mode.tick(0));
    mode.packet.fireMode = FIRE_BURST;
    mode.sync();
    CHECK(mode.fire.pendingCount() == 0);
    mode.packet.fireEpoch = mode.fire.epoch;
    mode.send(1, .02);
    CHECK(!mode.tick(.08)); // faster mode cannot shorten a shot's existing cooldown
    CHECK(mode.tick(.14));

    Fixture death;
    death.send(3);
    old = death.fire.epoch;
    death.gs.players[0].alive = false;
    death.sync();
    CHECK(!death.tick(0));
    death.gs.players[0] = Player{};
    death.fire.reset(death.gs.players[0], death.epochs);
    death.send(4, 3); // old-life input delayed until after respawn
    CHECK(!death.tick(3));
    CHECK(death.fire.epoch != old);
    death.packet.fireEpoch = death.fire.epoch;
    death.send(1, 3);
    CHECK(death.tick(3));
    ServerFire replacement;
    replacement.reset(death.gs.players[0], death.epochs);
    CHECK(replacement.epoch != death.fire.epoch); // slot reuse gets a fresh epoch
}

static void reloadAndFailures() {
    Fixture f;
    f.send(3);
    CHECK(f.tick(0));
    updateReload(f.gs.players[0], true, 1.0f / PHYS_HZ);
    CHECK(f.gs.players[0].reloading);
    f.sync();
    CHECK(f.fire.pendingCount() == 0);
    f.packet.fireEpoch = f.fire.epoch;
    f.send(1, .02); // cannot queue a shot while reloading
    CHECK(!f.tick(.02));
    uint32_t duringReload = f.fire.epoch;
    updateReload(f.gs.players[0], false, 2);
    f.sync();
    CHECK(!f.gs.players[0].reloading);
    CHECK(f.gs.players[0].mag == UZI.magSize);
    CHECK(f.gs.players[0].reserve == UZI.reservePerLife - 1);
    CHECK(f.fire.epoch != duringReload);
    f.send(2, 2); // delayed request from during reload
    CHECK(!f.tick(2));
    f.packet.fireEpoch = f.fire.epoch;
    f.send(1, 2);
    CHECK(f.tick(2));

    Fixture empty;
    empty.gs.players[0].mag = 0;
    empty.send(1);
    CHECK(!empty.tick(0));
    updateReload(empty.gs.players[0], false, 1.0f / PHYS_HZ);
    CHECK(empty.gs.players[0].reloading); // auto reload remains intact
    CHECK(empty.fire.shotsFired == 0);

    Fixture full;
    for (auto& b : full.gs.bullets) b.active = true;
    full.send(1);
    CHECK(!full.tick(0));
    CHECK(full.gs.players[0].mag == UZI.magSize && full.fire.shotsFired == 0);
    full.gs.bullets[0].active = false;
    CHECK(!full.tick(1)); // exhausted-pool request isn't retried later
}

static uint32_t seenView;
static float lookup(uint32_t seq, uint8_t) { seenView = seq; return .25f; }
static void aimAndValidation() {
    Fixture f;
    f.packet.viewSeq = 42;
    f.send(1);
    f.packet.yaw = 90;
    f.send(1); // newer movement / duplicate count cannot replace queued aim
    CHECK(f.fire.tick(f.gs, 0, 0, InputState{}, lookup));
    CHECK(seenView == 42);
    CHECK(f.gs.bullets[0].vel.x > 399 && std::fabs(f.gs.bullets[0].vel.z) < .01);
    CHECK(f.gs.bullets[0].compRewind == .25f);

    InputPacket decodedPacket = f.packet;
    decodedPacket.keys = KEY_W | KEY_SPRINT | KEY_JUMP | KEY_CROUCH;
    decodedPacket.flags = FLAG_ADS | FLAG_RELOAD;
    decodedPacket.yaw = 810;
    decodedPacket.lean = -127;
    InputState decoded = decodeFireInput(decodedPacket);
    CHECK(decoded.w && decoded.sprint && decoded.jump && decoded.crouch);
    CHECK(decoded.ads && decoded.reload && decoded.yaw == 90 && decoded.lean == -1);
    CHECK(aimSpread(f.gs.players[0], decoded) == SPRINT_SPREAD_DEG * ADS_MOVE_MULT);

    InputPacket p = f.packet;
    p.pitch = std::numeric_limits<float>::quiet_NaN(); CHECK(!validFireInput(p));
    p = f.packet; p.yaw = std::numeric_limits<float>::infinity(); CHECK(!validFireInput(p));
    p = f.packet; p.pitch = 90; CHECK(!validFireInput(p));
    p = f.packet; p.lean = -128; CHECK(!validFireInput(p));
    p = f.packet; p.weaponId = 255; CHECK(!validFireInput(p));
    p = f.packet; p.fireMode = 255; CHECK(!validFireInput(p));
    p = f.packet; p.weaponId = WEP_GLOCK19; p.fireMode = FIRE_AUTO; CHECK(!validFireInput(p));
    p = f.packet; p.yaw = 1e30f; CHECK(validFireInput(p)); // finite yaw normalized on receipt
}

static void rewindRegression() {
    GameState gs{};
    gs.usedMask = 1;
    gs.players[0].yaw = 179;
    serverTime = 1;
    recordSnapshot(gs);
    recordStateTime(10);
    gs.players[0].pos.x = 10;
    gs.players[0].yaw = -179;
    serverTime = 1.1;
    recordSnapshot(gs);
    glm::vec3 pos;
    bool crouched, ads, alive;
    float yaw, pitch, lean;
    uint8_t weapon;
    CHECK(rewindLookup(nullptr, 0, .05f, pos, crouched, yaw, pitch, lean, ads, weapon, alive));
    CHECK(std::fabs(pos.x - 5) < .001 && std::fabs(yaw - 180) < .001);
    CHECK(std::fabs(rewindForShot(10, 0) - .1f) < .0001);
    CHECK(rewindForShot(999, 0) == 0);
    serverTime = 5;
    CHECK(rewindForShot(10, 0) == 1.4f);
}

int main() {
    gTerrainMode = TERRAIN_OFF;
    cadence();
    queueAndSerials();
    stateChanges();
    reloadAndFailures();
    aimAndValidation();
    rewindRegression();
    if (failures) return 1;
    std::puts("server fire tests passed: cadence, queues, epochs, reload, ammo, aim and validation");
    return 0;
}
