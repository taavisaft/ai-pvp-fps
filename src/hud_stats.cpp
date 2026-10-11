#include "hud.h"
#include "perf.h"
#include <cstdio>

void drawHUDStats(Renderer& r, const HudState& hud, bool online) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%d FPS", (int)(hud.fps + 0.5f));
    r.drawText(buf, -0.98f, 0.93f, 0.04f, {0.6f, 0.9f, 0.6f}, 0.8f);
    snprintf(buf, sizeof(buf), "FRAME %.2fMS  RENDER CPU %.2fMS", hud.frameMs, hud.renderCpuMs);
    r.drawText(buf, -0.98f, 0.88f, 0.028f, {0.65f, 0.85f, 0.75f}, 0.75f);
    char cpu[24], ram[24];
    if (hud.processUsage.cpuPercent >= 0)
        snprintf(cpu, sizeof(cpu), "%.1f%%", hud.processUsage.cpuPercent);
    else snprintf(cpu, sizeof(cpu), "--");
    if (hud.processUsage.residentMiB >= 0)
        snprintf(ram, sizeof(ram), "%.0f MIB", hud.processUsage.residentMiB);
    else snprintf(ram, sizeof(ram), "--");
    snprintf(buf, sizeof(buf), "GAME CPU %s  RAM %s", cpu, ram);
    r.drawText(buf, -0.98f, 0.84f, 0.028f, {0.65f, 0.85f, 0.75f}, 0.75f);
    if (gProfiler.showHud) {
        snprintf(buf, sizeof(buf), "Q=%s  sh=%d  L0=%d L1=%d imp=%d",
                 gQuality.name, gQuality.shadowSize,
                 gVegStats.treesL0, gVegStats.treesL1, gVegStats.treesImp);
        r.drawText(buf, -0.98f, 0.80f, 0.026f, {0.70f, 0.80f, 0.90f}, 0.72f);
        float y = 0.76f;
        for (int p = 0; p < PASS_COUNT; p++) {
            snprintf(buf, sizeof(buf), "%s %.2fms", FrameProfiler::passName((RenderPass)p),
                     gProfiler.passMs[p]);
            r.drawText(buf, -0.98f, y, 0.024f, {0.75f, 0.78f, 0.82f}, 0.70f);
            y -= 0.038f;
        }
    }

    if (!online)
        r.drawText("PRACTICE - C TO CONNECT",
                   -0.98f, gProfiler.showHud ? 0.52f : 0.79f, 0.04f,
                   {0.8f, 0.8f, 0.8f}, 0.8f);

    snprintf(buf, sizeof(buf), "WEATHER: %s (K)", r.atmosphereName());
    r.drawText(buf, -0.98f, gProfiler.showHud ? 0.46f : 0.73f, 0.028f,
               {0.8f, 0.85f, 0.9f}, 0.85f);

}
