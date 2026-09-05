#include "server_fire.h"
#include "physics.h"
#include <cmath>

bool validFireInput(const InputPacket& p) {
    return p.weaponId < WEP_COUNT && p.fireMode < FIRE_MODE_COUNT &&
           (!weaponDef(p.weaponId).semiOnly || p.fireMode == FIRE_SEMI) &&
           std::isfinite(p.yaw) && std::isfinite(p.pitch) &&
           p.pitch >= -89.0f && p.pitch <= 89.0f && p.lean >= -127;
}

InputState decodeFireInput(const InputPacket& p) {
    InputState in{};
    in.w = p.keys & KEY_W; in.a = p.keys & KEY_A;
    in.s = p.keys & KEY_S; in.d = p.keys & KEY_D;
    in.sprint = p.keys & KEY_SPRINT;
    in.jump = p.keys & KEY_JUMP;
    in.crouch = p.keys & KEY_CROUCH;
    in.ads = p.flags & FLAG_ADS;
    in.reload = p.flags & FLAG_RELOAD;
    in.weaponId = p.weaponId;
    in.fireMode = p.fireMode;
    in.lean = p.lean / 127.0f;
    in.yaw = std::remainder(p.yaw, 360.0f);
    in.pitch = p.pitch;
    return in;
}

void ServerFire::invalidate(uint32_t& source) {
    if (++source == 0) ++source;   // zero means no server state received yet
    epoch = source;
    head = count = 0;
    lastRequest = 0;
}

void ServerFire::reset(const Player& p, uint32_t& source) {
    weapon = p.weaponId;
    mode = FIRE_SEMI;
    alive = p.alive;
    reloading = p.reloading;
    // Keep the session's cosmetic counter and cooldown across respawns.
    invalidate(source);
}

void ServerFire::synchronize(const Player& p, uint8_t desiredMode, uint32_t& source) {
    uint8_t nextMode = weaponDef(p.weaponId).semiOnly ? uint8_t(FIRE_SEMI) : desiredMode;
    if (nextMode >= FIRE_MODE_COUNT) nextMode = FIRE_SEMI;
    if (weapon == p.weaponId && mode == nextMode && alive == p.alive &&
        reloading == p.reloading) return;
    weapon = p.weaponId;
    mode = nextMode;
    alive = p.alive;
    reloading = p.reloading;
    // Do not reset nextAllowed: toggling mode or swapping cannot bypass cooldown.
    invalidate(source);
}

void ServerFire::receive(const InputPacket& p, const Player& player, double now) {
    if (!validFireInput(p) || p.fireEpoch != epoch ||
        !serialNewer(p.shotSeq, lastRequest)) return;
    uint32_t delta = p.shotSeq - lastRequest;
    lastRequest = p.shotSeq;  // rejected requests must not be retried every packet
    if (!player.alive || player.reloading || player.mag <= 0 ||
        player.weaponId != weapon || p.weaponId != weapon || p.fireMode != mode ||
        (p.flags & FLAG_RELOAD)) return;
    // Reject oversized jumps/batches outright; never truncate them into free shots.
    if (delta > MAX_PENDING || delta > uint32_t(MAX_PENDING - count)) return;
    Request request;
    request.yaw = std::remainder(p.yaw, 360.0f);
    request.pitch = p.pitch;
    request.lean = p.lean / 127.0f;
    request.ads = (p.flags & FLAG_ADS) != 0;
    request.viewSeq = p.viewSeq;
    request.viewFrac = p.viewFrac;
    request.expires = now + MAX_AGE;
    // A cumulative counter cannot recover distinct lost aim samples. Missing shots
    // use this packet's aim, as before, but queued aim is never overwritten later.
    for (uint32_t i = 0; i < delta; ++i) {
        pending[(head + count) % MAX_PENDING] = request;
        ++count;
    }
}

bool ServerFire::tick(GameState& gs, int id, double now, const InputState& movement, ShotRewind rewind) {
    Player& p = gs.players[id];
    if (!p.alive || p.reloading || p.mag <= 0 || p.weaponId != weapon) {
        head = count = 0;
        return false;
    }
    while (count && pending[head].expires < now) {
        head = (head + 1) % MAX_PENDING;
        --count;
    }
    if (!count || now + 1e-8 < nextAllowed) return false;
    Request request = pending[head];
    head = (head + 1) % MAX_PENDING;
    --count;
    const WeaponDef& wd = weaponDef(weapon);
    float interval = mode == FIRE_AUTO ? wd.fireAutoInt :
                     mode == FIRE_BURST ? wd.fireBurstInt : wd.fireSemiInt;
    nextAllowed = now + interval;  // never bank unused cooldown time for catch-up firing

    InputState aim = movement;
    aim.crouch = p.crouched;
    aim.yaw = request.yaw;
    aim.pitch = request.pitch;
    aim.lean = request.lean;
    aim.ads = request.ads;
    // Preserve the movement input used by this server tick when computing spread.
    float eyeH = p.crouched ? CROUCH_EYE : EYE_HEIGHT;
    glm::vec3 eye = p.pos + glm::vec3(0, eyeH, 0);
    LeanShift ls = leanShift(aim.lean);
    float yr = glm::radians(aim.yaw);
    eye += glm::vec3(-sinf(yr), 0.0f, cosf(yr)) * ls.lateral;
    eye.y -= ls.drop;
    glm::vec3 origin, dir;
    weaponShot(aim.yaw, aim.pitch, eye, aim.ads, aimSpread(p, aim), origin, dir);
    float comp = rewind ? rewind(request.viewSeq, request.viewFrac) : 0.0f;
    if (!spawnBullet(gs, origin, dir, id, comp)) return false;
    ++shotsFired;
    return true;
}
