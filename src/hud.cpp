#include "hud.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static constexpr float FEED_TTL  = 4.0f;
static constexpr float TEXT_H    = 0.05f;   // standard char height in NDC

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

static void drawScoreboard(Renderer& r, const GameState& gs, int localID) {
    int order[MAX_PLAYERS], n = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (gs.usedMask & (1u << i)) order[n++] = i;
    for (int i = 1; i < n; i++) {            // insertion sort by kills desc
        int v = order[i], j = i - 1;
        while (j >= 0 && gs.players[order[j]].kills < gs.players[v].kills) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = v;
    }

    float rowH = 0.07f, top = 0.55f;
    r.drawRect({0, top - rowH * (n + 1) * 0.5f + rowH * 0.75f},
               {0.85f, rowH * (n + 1) + 0.06f}, {0, 0, 0}, 0.6f);
    r.drawText("PLAYER   K   D", -0.35f, top, TEXT_H, {0.7f, 0.7f, 0.7f}, 0.9f);
    char line[32];
    for (int i = 0; i < n; i++) {
        const Player& p = gs.players[order[i]];
        snprintf(line, sizeof(line), "P%-6d %3d %3d", order[i], p.kills, p.deaths);
        glm::vec3 c = order[i] == localID ? glm::vec3(1.0f, 0.9f, 0.3f)
                                          : glm::vec3(0.9f, 0.9f, 0.9f);
        r.drawText(line, -0.35f, top - rowH * (i + 1), TEXT_H, c, 0.9f);
    }
}

void drawHUD(Renderer& r, const GameState& gs, int localID,
             const HudState& hud, bool scoreboard, bool online) {
    float ia = 1.0f / r.aspect();
    const Player& own = gs.players[localID];
    char buf[32];
    r.beginHUD();

    if (hud.flashTimer > 0.0f)
        r.drawRect({0, 0}, {2, 2}, {0.9f, 0.1f, 0.1f}, 0.35f * hud.flashTimer / 0.4f);

    float hpFrac = own.hp / (float)PLAYER_HP;
    glm::vec3 hpColor = glm::mix(glm::vec3(0.9f, 0.2f, 0.1f),
                                 glm::vec3(0.2f, 0.85f, 0.2f), hpFrac);
    drawBar(r, {-0.6f, -0.86f}, {0.5f, 0.05f}, hpFrac, hpColor);
    snprintf(buf, sizeof(buf), "%d", own.hp);
    r.drawText(buf, -0.33f, -0.885f, TEXT_H, hpColor, 0.9f);

    glm::vec3 ammoCol = {0.95f, 0.85f, 0.25f};
    drawBar(r, {-0.6f, -0.94f}, {0.5f, 0.03f}, own.mag / (float)MAG_SIZE, ammoCol);
    if (own.reloading) snprintf(buf, sizeof(buf), "RELOADING");
    else               snprintf(buf, sizeof(buf), "%d / %d", own.mag, own.reserve);
    r.drawText(buf, -0.33f, -0.96f, 0.04f, ammoCol, 0.9f);

    const char* modeStr = hud.fireMode == FIRE_AUTO  ? "AUTO"
                        : hud.fireMode == FIRE_BURST ? "BURST" : "SEMI";
    r.drawText(modeStr, 0.78f, -0.96f, 0.04f, {0.6f, 0.8f, 0.95f}, 0.85f);

    if (own.alive) {
        r.drawRect({0, 0}, {0.006f * ia, 0.045f}, {1, 1, 1}, 0.9f);
        r.drawRect({0, 0}, {0.045f * ia, 0.006f}, {1, 1, 1}, 0.9f);
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

    snprintf(buf, sizeof(buf), "%d FPS", (int)(hud.fps + 0.5f));
    r.drawText(buf, -0.98f, 0.93f, 0.04f, {0.6f, 0.9f, 0.6f}, 0.8f);

    if (!online)
        r.drawText("OFFLINE PRACTICE - PRESS C TO CONNECT",
                   -0.98f, 0.87f, 0.04f, {0.8f, 0.8f, 0.8f}, 0.8f);

    for (int i = 0; i < hud.feed.count; i++) {
        const KillFeed::Entry& e = hud.feed.entries[i];
        float a = e.ttl < 1.0f ? e.ttl : 1.0f;
        r.drawText(e.text, 0.98f - r.textWidth(e.text, 0.045f),
                   0.92f - 0.06f * i, 0.045f, {1, 0.6f, 0.3f}, 0.9f * a);
    }

    if (scoreboard) drawScoreboard(r, gs, localID);
    r.endHUD();
}
