#include "hud.h"
#include "map.h"
#include "perf.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static constexpr float FEED_TTL  = 4.0f;

void KillFeed::push(int killer, int victim) {
    if (count == 4) {
        memmove(&entries[0], &entries[1], sizeof(Entry) * 3);
        count = 3;
    }
    snprintf(entries[count].text, sizeof(entries[count].text),
             "P%d KILLED P%d", killer, victim);
    entries[count].ttl = FEED_TTL;
    count++;
}

void KillFeed::update(float dt) {
    while (count > 0 && (entries[0].ttl -= dt) <= 0.0f) {
        memmove(&entries[0], &entries[1], sizeof(Entry) * (count - 1));
        count--;
    }
    for (int i = 1; i < count; i++) entries[i].ttl -= dt;
}

void HudState::noteState(const StatePacket& s) {
    if (tracked) {
        // pair killers with victims in slot order; ambiguous only when several
        // kills land in the same 50 ms window — acceptable for a feed
        int killers[MAX_PLAYERS], victims[MAX_PLAYERS];
        int nk = 0, nv = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!(s.usedMask & (1u << i))) continue;
            for (int d = prevKills[i];  d < s.players[i].kills;  d++) killers[nk++] = i;
            for (int d = prevDeaths[i]; d < s.players[i].deaths; d++) victims[nv++] = i;
        }
        for (int i = 0; i < nk && i < nv; i++) feed.push(killers[i], victims[i]);
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        // joiners reset to 0; track only while the slot stays used
        prevKills[i]  = (s.usedMask & (1u << i)) ? s.players[i].kills  : 0;
        prevDeaths[i] = (s.usedMask & (1u << i)) ? s.players[i].deaths : 0;
    }
    tracked = true;
}

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

void drawScoreboard(Renderer& r, const GameState& gs, int localID);

// Top-down map view, north (+Z) up. Two callers:
//   corner minimap  -> playerCentric: local player fixed at panel center c, the
//                       world translates by -me.pos, zoomed to worldHalf meters.
//   full-screen map -> centered on the arena origin, whole map fits the panel,
//                       the local dot moves to its real position.
// Obstacle boxes (gMapBoxes) are clipped to the panel; off-window enemies clamp
// to the edge (gives bearing). yaw forward = (cos,sin) in world XZ = screen right,up.
static void drawMapView(Renderer& r, const GameState& gs, int localID,
                        glm::vec2 c, float halfY, float worldHalf, bool playerCentric,
                        unsigned int satTex, float satHalf) {
    float ia    = 1.0f / r.aspect();
    float halfX = halfY * ia;                 // square on screen
    const Player& me = gs.players[localID];
    float sx = halfX / worldHalf;             // NDC per meter, world X -> screen right
    float sy = halfY / worldHalf;             // NDC per meter, world Z -> screen up (north)
    float ex = playerCentric ? me.pos.x : 0.0f;   // world point at panel center
    float ez = playerCentric ? me.pos.z : 0.0f;

    r.drawRect(c, {2 * halfX + 0.018f, 2 * halfY + 0.018f}, {0.5f, 0.55f, 0.6f}, 0.35f);
    r.drawRect(c, {2 * halfX, 2 * halfY}, {0.06f, 0.07f, 0.09f}, playerCentric ? 0.55f : 0.82f);

    if (satTex) {
        // satellite image fills the panel; corner view samples the sub-rect of the
        // texture (baked over [-satHalf,satHalf]) that the world window covers.
        glm::vec2 uvC = { 0.5f + ex / (2.0f * satHalf), 0.5f + ez / (2.0f * satHalf) };
        glm::vec2 uvH = { worldHalf / (2.0f * satHalf), worldHalf / (2.0f * satHalf) };
        r.drawTexQuad(c, {2 * halfX, 2 * halfY}, satTex, 1.0f, uvC, uvH);
    } else {
        // fallback: gray obstacle footprints, clipped to the panel
        auto drawClipped = [&](glm::vec2 rc, glm::vec2 rs, glm::vec3 col, float a) {
            float x0 = fmaxf(rc.x - rs.x * 0.5f, c.x - halfX);
            float x1 = fminf(rc.x + rs.x * 0.5f, c.x + halfX);
            float y0 = fmaxf(rc.y - rs.y * 0.5f, c.y - halfY);
            float y1 = fminf(rc.y + rs.y * 0.5f, c.y + halfY);
            if (x1 <= x0 || y1 <= y0) return;
            r.drawRect({(x0 + x1) * 0.5f, (y0 + y1) * 0.5f}, {x1 - x0, y1 - y0}, col, a);
        };
        for (int i = 0; i < gMapBoxCount; i++) {
            const Box& b = gMapBoxes[i];
            glm::vec2 bc = { c.x + (b.center.x - ex) * sx, c.y + (b.center.z - ez) * sy };
            glm::vec2 bs = { 2.0f * b.half.x * sx, 2.0f * b.half.z * sy };
            drawClipped(bc, bs, {0.55f, 0.58f, 0.63f}, 0.85f);
        }
    }

    auto clampInside = [&](glm::vec2 p) {
        if (p.x < c.x - halfX) p.x = c.x - halfX;
        if (p.x > c.x + halfX) p.x = c.x + halfX;
        if (p.y < c.y - halfY) p.y = c.y - halfY;
        if (p.y > c.y + halfY) p.y = c.y + halfY;
        return p;
    };

    const float dot = 0.016f;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == localID) continue;
        if (!(gs.usedMask & (1u << i)) || !gs.players[i].alive) continue;
        const glm::vec3& pp = gs.players[i].pos;
        glm::vec2 m = clampInside({ c.x + (pp.x - ex) * sx, c.y + (pp.z - ez) * sy });
        r.drawRect(m, {dot * ia, dot}, {0.9f, 0.25f, 0.2f}, 0.95f);
    }

    // local player: heading arrow then a dot on top
    glm::vec2 mp = playerCentric
                 ? c
                 : clampInside({ c.x + me.pos.x * sx, c.y + me.pos.z * sy });
    float yawR = glm::radians(me.yaw);
    glm::vec2 dir = { cosf(yawR), sinf(yawR) };
    float len = 0.05f;
    glm::vec2 mid = { mp.x + dir.x * (len * 0.5f) * ia, mp.y + dir.y * (len * 0.5f) };
    r.drawRectRot(mid, {len, 0.012f}, {1.0f, 0.9f, 0.3f}, 0.95f, yawR);
    r.drawRect(mp, {dot * 1.2f * ia, dot * 1.2f}, {1.0f, 0.9f, 0.3f}, 1.0f);
}

static void drawCompass(Renderer& r, float yaw) {
    // Gameplay yaw is 0 at east and 90 at north; map north is +Z.
    float heading = fmodf(90.0f - yaw, 360.0f);
    if (heading < 0.0f) heading += 360.0f;
    r.drawRect({0, 0.925f}, {1.10f, 0.12f}, {0.02f, 0.03f, 0.04f}, 0.38f);
    for (int bearing = 0; bearing < 360; bearing += 15) {
        float delta = fmodf((float)bearing - heading + 540.0f, 360.0f) - 180.0f;
        if (fabsf(delta) > 85.0f) continue;
        float x = delta * (0.52f / 85.0f);
        bool major = bearing % 45 == 0;
        r.drawRect({x, 0.955f}, {0.003f, major ? 0.025f : 0.012f},
                   {0.92f, 0.94f, 0.95f}, major ? 0.9f : 0.55f);
        if (major) {
            char label[8];
            const char* cardinals[] = {"N", "E", "S", "W"};
            if (bearing % 90 == 0) snprintf(label, sizeof(label), "%s", cardinals[bearing / 90]);
            else                   snprintf(label, sizeof(label), "%d", bearing);
            r.drawText(label, x - r.textWidth(label, 0.036f) * 0.5f,
                       0.88f, 0.036f, {0.9f, 0.92f, 0.94f}, 0.9f);
        }
    }
    r.drawRect({0, 0.985f}, {0.010f, 0.018f}, {1.0f, 0.9f, 0.36f}, 1.0f);
}

void drawHUD(Renderer& r, const GameState& gs, int localID,
             const HudState& hud, bool scoreboard, bool online, bool fullMap) {
    float ia = 1.0f / r.aspect();
    const Player& own = gs.players[localID];
    char buf[32];
    r.beginHUD();

    if (hud.flashTimer > 0.0f)
        r.drawRect({0, 0}, {2, 2}, {0.9f, 0.1f, 0.1f}, 0.35f * hud.flashTimer / 0.4f);

    float hpFrac = own.hp / (float)PLAYER_HP;
    glm::vec3 hpColor = hpFrac < 0.3f ? glm::vec3(0.95f, 0.35f, 0.25f)
                                       : glm::vec3(0.94f, 0.95f, 0.94f);
    drawBar(r, {0.02f, -0.945f}, {0.48f, 0.035f}, hpFrac, hpColor);
    snprintf(buf, sizeof(buf), "%d", own.hp);
    r.drawText(buf, -0.31f, -0.94f, 0.045f, hpColor, 0.95f);

    const WeaponDef& lw = weaponDef(gWeaponId);   // local player's selected weapon
    if (own.reloading) snprintf(buf, sizeof(buf), "%s  RELOADING", lw.name);
    else               snprintf(buf, sizeof(buf), "%s  %d / %d", lw.name, own.mag, own.reserve);
    r.drawText(buf, -r.textWidth(buf, 0.058f) * 0.5f, -0.88f, 0.058f,
               {0.96f, 0.96f, 0.94f}, 0.95f);

    // Fire mode shown only briefly after a change (B), centered low.
    if (hud.fireModeTimer > 0.0f) {
        const char* modeStr = hud.fireMode == FIRE_AUTO  ? "AUTO"
                            : hud.fireMode == FIRE_BURST ? "BURST" : "SEMI";
        float a = hud.fireModeTimer > 1.0f ? 1.0f : hud.fireModeTimer;  // fade last 1s
        r.drawText(modeStr, -r.textWidth(modeStr, 0.05f) * 0.5f, -0.78f, 0.05f,
                   {0.7f, 0.85f, 1.0f}, a);
    }

    if (own.alive) {
        if (gWeaponId == WEP_UZI) {
            // Hipfire crosshair fades out; red-dot sight picture fades in when ADS.
            float ads  = hud.adsT;
            float hipA = 1.0f - (ads * 2.0f > 1.0f ? 1.0f : ads * 2.0f);
            if (hipA > 0.01f) {
                r.drawRect({0, 0}, {0.006f * ia, 0.045f}, {1, 1, 1}, 0.9f * hipA);
                r.drawRect({0, 0}, {0.045f * ia, 0.006f}, {1, 1, 1}, 0.9f * hipA);
            }
            float dotA = (ads - 0.25f) * 1.5f;
            if (dotA < 0.0f) dotA = 0.0f;
            if (dotA > 1.0f) dotA = 1.0f;
            if (dotA > 0.01f)
                r.drawRect({0, 0}, {0.009f * ia, 0.009f}, {0.95f, 0.12f, 0.08f}, 0.95f * dotA);
        } else {
            r.drawRect({0, 0}, {0.006f * ia, 0.045f}, {1, 1, 1}, 0.9f);
            r.drawRect({0, 0}, {0.045f * ia, 0.006f}, {1, 1, 1}, 0.9f);
        }
    }

    // Hit marker: a small X at the world impact point you last hit (projected
    // to screen in main.cpp). Two diagonal strokes, fading out over its lifetime.
    if (hud.hitMarkerTimer > 0.0f && hud.hitMarkerOnScreen) {
        float a = hud.hitMarkerTimer / HIT_MARKER_TIME;
        if (a > 1.0f) a = 1.0f;
        glm::vec2 sz = {0.045f, 0.009f};
        r.drawRectRot(hud.hitMarkerNDC, sz, {1.0f, 1.0f, 0.9f}, a,  0.7854f);
        r.drawRectRot(hud.hitMarkerNDC, sz, {1.0f, 1.0f, 0.9f}, a, -0.7854f);
    }

    if (!own.alive) {
        r.drawRect({0, 0}, {2, 2}, {0.6f, 0.05f, 0.05f}, 0.4f);
        snprintf(buf, sizeof(buf), "RESPAWN IN %d", (int)ceilf(hud.deathTimer));
        r.drawText(buf, -r.textWidth(buf, 0.08f) * 0.5f, -0.04f, 0.08f,
                   {1, 1, 1}, 0.95f);
    }

    drawCompass(r, own.yaw);
    snprintf(buf, sizeof(buf), "%d FPS", (int)(hud.fps + 0.5f));
    r.drawText(buf, -0.98f, 0.93f, 0.04f, {0.6f, 0.9f, 0.6f}, 0.8f);
    snprintf(buf, sizeof(buf), "FRAME %.2fMS  RENDER CPU %.2fMS", hud.frameMs, hud.renderCpuMs);
    r.drawText(buf, -0.98f, 0.88f, 0.028f, {0.65f, 0.85f, 0.75f}, 0.75f);
    if (gProfiler.showHud) {
        snprintf(buf, sizeof(buf), "Q=%s  sh=%d  L0=%d L1=%d imp=%d",
                 gQuality.name, gQuality.shadowSize,
                 gVegStats.treesL0, gVegStats.treesL1, gVegStats.treesImp);
        r.drawText(buf, -0.98f, 0.84f, 0.026f, {0.70f, 0.80f, 0.90f}, 0.72f);
        float y = 0.80f;
        for (int p = 0; p < PASS_COUNT; p++) {
            snprintf(buf, sizeof(buf), "%s %.2fms", FrameProfiler::passName((RenderPass)p),
                     gProfiler.passMs[p]);
            r.drawText(buf, -0.98f, y, 0.024f, {0.75f, 0.78f, 0.82f}, 0.70f);
            y -= 0.038f;
        }
    }

    if (!online)
        r.drawText("PRACTICE - C TO CONNECT",
                   -0.98f, gProfiler.showHud ? 0.56f : 0.83f, 0.04f,
                   {0.8f, 0.8f, 0.8f}, 0.8f);

    int alive = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if ((gs.usedMask & (1u << i)) && gs.players[i].alive) alive++;
    snprintf(buf, sizeof(buf), "%d ALIVE", alive);
    r.drawRect({0.875f, 0.93f}, {0.21f, 0.075f}, {0.02f, 0.03f, 0.04f}, 0.6f);
    r.drawText(buf, 0.875f - r.textWidth(buf, 0.048f) * 0.5f,
               0.905f, 0.048f, {0.96f, 0.96f, 0.94f}, 0.95f);

    for (int i = 0; i < hud.feed.count; i++) {
        const KillFeed::Entry& e = hud.feed.entries[i];
        float a = e.ttl < 1.0f ? e.ttl : 1.0f;
        r.drawText(e.text, 0.98f - r.textWidth(e.text, 0.045f),
                   0.92f - 0.06f * i, 0.045f, {1, 0.6f, 0.3f}, 0.9f * a);
    }

    float satHalf = mapViewHalf();   // extent the satellite texture is baked over
    unsigned int satTex = r.mapTexture(gMapId, gMapBoxes, gMapBoxCount, satHalf);
    if (fullMap) {
        r.drawRect({0, 0}, {2, 2}, {0, 0, 0}, 0.5f);          // dim the world behind
        drawMapView(r, gs, localID, {0.0f, -0.03f}, 0.72f, satHalf, false, satTex, satHalf);
        const char* t = "MAP - M TO CLOSE";
        r.drawText(t, -r.textWidth(t, 0.045f) * 0.5f, 0.78f, 0.045f,
                   {0.8f, 0.8f, 0.85f}, 0.9f);
    } else {
        float ia = 1.0f / r.aspect();
        float halfY = 0.16f, halfX = halfY * ia, margin = 0.035f;
        glm::vec2 c = { 1.0f - margin - halfX, -1.0f + margin + halfY };
        drawMapView(r, gs, localID, c, halfY, 100.0f, true, satTex, satHalf);
    }

    if (scoreboard) drawScoreboard(r, gs, localID);
    r.endHUD();
}
