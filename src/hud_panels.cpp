#include "hud.h"
#include "connect_prompt.h"
#include "lobby.h"
#include <cstdio>

static constexpr float TEXT_H = 0.05f;

void drawScoreboard(Renderer& r, const GameState& gs, int localID) {
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

// Stage 1: type the host address. Enter starts the lobby scan.
static void drawIpEntry(Renderer& r, const ConnectPrompt& prompt) {
    r.drawRect({0, 0.02f}, {1.1f, 0.42f}, {0.08f, 0.08f, 0.10f}, 0.92f);
    r.drawText("SERVER IP", -0.48f, 0.16f, 0.055f, {0.75f, 0.75f, 0.75f}, 1.0f);
    r.drawRect({0, -0.02f}, {0.92f, 0.12f}, {0.18f, 0.18f, 0.22f}, 1.0f);
    if (prompt.ip[0] == '\0')
        // dim hint — not real content; typing builds the IP from scratch
        r.drawText("127.0.0.1", -0.43f, -0.05f, 0.065f, {0.40f, 0.40f, 0.45f}, 1.0f);
    else
        r.drawText(prompt.ip, -0.43f, -0.05f, 0.065f, {1, 1, 1}, 1.0f);
    r.drawText("ENTER SCAN   ESC CANCEL", -0.48f, -0.16f, 0.04f,
               {0.55f, 0.55f, 0.55f}, 0.95f);
}

// Stage 2: list every map server the probe answered for. Up/Down + Enter to join.
static void drawServerBrowser(Renderer& r, const ConnectPrompt& prompt, const Lobby& lobby) {
    const float rowH = 0.11f, top = 0.24f;
    int   n     = lobby.count;
    float panelH = 0.34f + rowH * (n > 0 ? n : 1);
    r.drawRect({0, 0.02f}, {1.3f, panelH}, {0.08f, 0.08f, 0.10f}, 0.94f);

    char head[80];
    snprintf(head, sizeof(head), "GAMES ON %s", lobby.host);
    r.drawText(head, -0.6f, top + 0.07f, 0.05f, {0.75f, 0.78f, 0.82f}, 1.0f);

    if (n == 0) {
        r.drawText("scanning...", -0.6f, top - rowH, 0.05f, {0.55f, 0.55f, 0.6f}, 1.0f);
    }
    for (int i = 0; i < n; i++) {
        const ServerEntry& e = lobby.entries[i];
        float y   = top - rowH * (i + 1);
        bool  hot = (i == prompt.sel);
        if (hot) {
            r.drawRect({0, y + 0.018f}, {1.22f, rowH * 0.92f}, {0.20f, 0.30f, 0.42f}, 0.9f);
            r.drawText(">", -0.62f, y, 0.05f, {1, 1, 0.4f}, 1.0f);
        }
        glm::vec3 col = hot ? glm::vec3{1, 1, 1} : glm::vec3{0.78f, 0.78f, 0.82f};
        r.drawText(e.name, -0.56f, y, 0.05f, col, 1.0f);

        char pop[16];
        snprintf(pop, sizeof(pop), "%d/%d", e.players, e.maxPlayers);
        r.drawText(pop, 0.18f, y, 0.05f, col, 1.0f);

        char ping[16];
        if (e.pingMs >= 0.0f) snprintf(ping, sizeof(ping), "%dMS", (int)(e.pingMs + 0.5f));
        else                  snprintf(ping, sizeof(ping), "--");
        r.drawText(ping, 0.46f, y, 0.05f, {0.55f, 0.7f, 0.55f}, 1.0f);
    }
    r.drawText("UP/DOWN SELECT   ENTER JOIN   ESC CANCEL",
               -0.6f, top - rowH * (n > 0 ? n : 1) - 0.06f, 0.035f,
               {0.55f, 0.55f, 0.55f}, 0.95f);
}

void drawConnectPrompt(Renderer& r, const ConnectPrompt& prompt, const Lobby& lobby) {
    if (!prompt.open) return;
    r.beginHUD();
    r.drawRect({0, 0}, {2, 2}, {0, 0, 0}, 0.55f);
    if (prompt.mode == PM_BROWSE) drawServerBrowser(r, prompt, lobby);
    else                          drawIpEntry(r, prompt);
    r.endHUD();
}
