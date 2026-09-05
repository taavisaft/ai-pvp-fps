#include "server_rewind.h"
#include <cmath>

// --- Lag compensation: rewind player hitboxes to the shooter's view-time ---
constexpr int   HISTORY_TICKS   = 90;    // 1.5 s of position history at 60 Hz
constexpr float HISTORY_SECONDS = 1.4f;  // max rewind, kept under the buffer
constexpr int   STATE_HIST      = 64;    // recent state-seq -> send-time map size

double serverTime = 0.0;          // monotonic seconds since boot

// One per-tick snapshot of every player's hitbox, tagged with its server time.
struct Snap {
    float     t = 0.0f;
    uint16_t  usedMask = 0;
    glm::vec3 pos[MAX_PLAYERS];
    bool      crouched[MAX_PLAYERS];
    bool      alive[MAX_PLAYERS];
    float     yaw[MAX_PLAYERS];     // hit regions are yaw-oriented
    float     pitch[MAX_PLAYERS];   // neck aim tilt moves the head box
    float     lean[MAX_PLAYERS];    // torso lean roll moves the upper-body boxes
    bool      ads[MAX_PLAYERS];     // raises the arms box when aiming
    uint8_t   weaponId[MAX_PLAYERS];// gun + hand-grip boxes are weapon-shaped
};
static Snap history[HISTORY_TICKS];
static int  histHead  = -1;   // index of newest snapshot
static int  histCount = 0;    // valid snapshots in the ring

// Maps a broadcast state seq to the server time it was sent, so an incoming
// viewSeq can be turned into an absolute server time.
struct StateStamp { uint32_t seq = 0xFFFFFFFFu; float t = 0.0f; };
static StateStamp stateStamps[STATE_HIST];

static bool stateTimeFor(uint32_t seq, float& outT) {
    const StateStamp& s = stateStamps[seq % STATE_HIST];
    if (s.seq != seq) return false;   // unknown / overwritten
    outT = s.t;
    return true;
}

void recordSnapshot(const GameState& game) {
    histHead = (histHead + 1) % HISTORY_TICKS;
    Snap& s = history[histHead];
    s.t        = serverTime;
    s.usedMask = game.usedMask;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        s.pos[i]      = game.players[i].pos;
        s.crouched[i] = game.players[i].crouched;
        s.alive[i]    = game.players[i].alive;
        s.yaw[i]      = game.players[i].yaw;
        s.pitch[i]    = game.players[i].pitch;
        s.lean[i]     = game.players[i].lean;
        s.ads[i]      = game.players[i].ads;
        s.weaponId[i] = game.players[i].weaponId;
    }
    if (histCount < HISTORY_TICKS) histCount++;
}

// RewindLookup: where player `pid`'s hitbox was `rewindSec` seconds ago.
bool rewindLookup(const void* ctx, int pid, float rewindSec,
                         glm::vec3& pos, bool& crouched, float& yaw, float& pitch,
                         float& lean, bool& ads, uint8_t& weaponId, bool& alive) {
    (void)ctx;
    if (histCount == 0) return false;
    float    target = serverTime - rewindSec;
    uint16_t bit    = (uint16_t)(1u << pid);

    int   olderIdx = histHead, newerIdx = histHead;
    float a = 0.0f;
    const Snap& newest = history[histHead];
    if (target >= newest.t || histCount == 1) {
        olderIdx = newerIdx = histHead;          // at/after newest: no interp
    } else {
        bool found = false;
        for (int k = 1; k < histCount; k++) {
            int oi = (histHead - k + HISTORY_TICKS) % HISTORY_TICKS;
            if (history[oi].t <= target) {
                olderIdx = oi;
                newerIdx = (histHead - (k - 1) + HISTORY_TICKS) % HISTORY_TICKS;
                float span = history[newerIdx].t - history[oi].t;
                a = span > 1e-6f ? (target - history[oi].t) / span : 0.0f;
                found = true;
                break;
            }
        }
        if (!found)                              // older than everything retained
            olderIdx = newerIdx = (histHead - (histCount - 1) + HISTORY_TICKS) % HISTORY_TICKS;
    }

    const Snap& sa = history[olderIdx];
    const Snap& sb = history[newerIdx];
    if (!(sb.usedMask & bit) || !sb.alive[pid]) return false;  // not a valid target then
    bool      olderValid = (sa.usedMask & bit) && sa.alive[pid];
    glm::vec3 older = olderValid ? sa.pos[pid] : sb.pos[pid];
    pos      = glm::mix(older, sb.pos[pid], a);
    crouched = sb.crouched[pid];
    ads      = sb.ads[pid];
    // Shortest-arc yaw interp so a player spinning across the 0/360 seam doesn't make
    // the hit regions swing the long way round between snapshots.
    float ya = olderValid ? sa.yaw[pid] : sb.yaw[pid];
    float dy = sb.yaw[pid] - ya;
    while (dy > 180.0f)  dy -= 360.0f;
    while (dy < -180.0f) dy += 360.0f;
    yaw      = ya + dy * a;
    // Pitch is clamped to ±89 and lean to ±1 — no seam, plain lerp.
    pitch    = glm::mix(olderValid ? sa.pitch[pid] : sb.pitch[pid], sb.pitch[pid], a);
    lean     = glm::mix(olderValid ? sa.lean[pid]  : sb.lean[pid],  sb.lean[pid],  a);
    weaponId = sb.weaponId[pid];
    alive    = true;
    return true;
}

void recordStateTime(uint32_t seq) {
    stateStamps[seq % STATE_HIST] = {seq, (float)serverTime};
}

float rewindForShot(uint32_t seq, uint8_t fraction) {
    float viewT;
    if (!stateTimeFor(seq, viewT)) return 0.0f;
    float comp = (float)(serverTime - (viewT + (fraction / 255.0f) / NET_HZ));
    return fmaxf(0.0f, fminf(comp, HISTORY_SECONDS));
}
