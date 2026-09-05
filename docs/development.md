# Development notes

[Back to the project](../README.md) · [Roadmap](../TODO.md) · [Contributor guide](../AGENTS.md)

## Controls

| Input        | Action                            |
| ------------ | --------------------------------- |
| W A S D      | Move                              |
| Shift        | Sprint                            |
| Space        | Jump                              |
| Left Ctrl    | Crouch                            |
| Q / E        | Lean left / right (peek)          |
| Mouse        | Look                              |
| Left click   | Shoot                             |
| Right mouse  | Aim down sights (zoom)            |
| 1 / 2        | Select weapon (Uzi / Glock 19)    |
| Scroll wheel | Cycle weapon                      |
| B            | Cycle fire mode (Uzi: semi/burst/auto; Glock is semi-only) |
| R            | Reload                            |
| Tab (hold)   | Scoreboard                        |
| G            | Clear bullet marks (offline)      |
| C            | Connect — host prompt, then server browser |
| M            | Toggle full-screen map            |
| J            | Toggle HUD on / off (immersion)   |
| K            | Cycle atmosphere (clear / overcast / golden hour) |
| V            | Toggle third-person self-view (see your own body) |
| H            | Toggle hitbox overlay (translucent green, debug) |
| F            | Toggle wireframe                  |
| ESC          | Quit                              |

HP, ammo, and the current weapon show on screen, kills appear in the feed top-right, and holding Tab shows the scoreboard. Press **J** to hide the entire HUD for a clean, immersive view; press again to restore it.


### Testing netcode (lag, jitter, packet loss)

The client can simulate a bad connection so you can test lag compensation and the
interpolation buffer on a single machine. Set any combination of these environment
variables on the client; they apply to packets in both directions:

| Variable     | Meaning                                  | Example       |
| ------------ | ---------------------------------------- | ------------- |
| `FPS_LAG`    | One-way latency, milliseconds            | `FPS_LAG=150` |
| `FPS_JITTER` | Random ±delay added per packet, ms       | `FPS_JITTER=40` |
| `FPS_LOSS`   | Packet drop chance, percent (0–95)       | `FPS_LOSS=10` |

```bash
# terminal 1
./build/server

# terminal 2 — 150 ms one-way (300 ms RTT), ±40 ms jitter, 10% loss
FPS_LAG=150 FPS_JITTER=40 FPS_LOSS=10 ./build/game 127.0.0.1
```

- **Lag compensation:** the server uses the client's rendered snapshot time to rewind target hitboxes. Moving targets, cover interactions and the rewind cap still need repeatable combat tests.
- **Interpolation buffer:** remote players render a couple of snapshots behind the newest packet, so jitter and the occasional dropped packet stay smooth instead of stuttering or snapping.

Without these variables, the artificial delay/loss queues are disabled; real network latency still applies. Local movement prediction currently smooths server corrections and does not yet replay unacknowledged inputs.


## Atmosphere

The whole scene runs through a filmic tonemap (ACES) with drifting **cloud shadows**
sweeping over the terrain and **height fog** that pools in valleys while hilltops poke
out of the haze. Press **K** to cycle three moods: **clear** (bright midday), **overcast**
(grey DayZ-style gloom — weak flat sun, close fog, desaturated colors), and **golden
hour** (low warm sun, long shadows, amber horizon). Set `FPS_ATMO=clear|overcast|golden`
to pick the starting mood. Atmosphere is client-side and cosmetic — every player can run
their own sky.


## Performance and reference views

```bash
# Automated fixed forest view: 300 warmup frames, then 600 measured frames
FPS_REF=forest FPS_QUALITY=medium FPS_BENCH=1 ./build-release/game

# Capture the bog view at frame 60 and exit (use an absolute output path)
FPS_REF=bog FPS_PERF=1 FPS_SHOT=/tmp/fps-bog.ppm ./build-release/game
```

`FPS_REF` presets: `shore`, `bog`, `forest`, `ridge`, `golden`. A reference preset
selects Paldiski offline. `FPS_QUALITY` accepts `low`, `medium` (default), or `high`;
`FPS_PERF=1` shows CPU pass timings and vegetation counts. Automated captures and
benchmarks pin the camera; ordinary reference launches remain controllable.
`FPS_REF=all` currently selects the first preset only; it does not capture a suite.
Do not combine `FPS_SHOT` with `FPS_BENCH`: the screenshot exits before sampling.

Compare the same build type, hardware, drawable resolution, quality and viewpoint.
Frame samples use uncapped wall time; pass timers measure CPU submission/wait,
not GPU execution. See the [dated benchmark](progress-2026-09-05.md) for the
HUD improvement, including its limited test scope.


## Tests and assets

Run `ctest --test-dir build --output-on-failure` after building. See [server firing tests](server-firing-2026-09-05.md) and [Blender asset instructions](../tools/soldier/README.md).

## Platform setup

On Ubuntu/Debian: `sudo apt install cmake build-essential libsdl2-dev libgl1-mesa-dev`. Windows requires SDL2 and an OpenGL-capable C++17 toolchain. macOS has been exercised locally; Linux/Windows parity still needs verification.

## Repository media

The README screenshot was captured directly at 2560×1440, high quality, golden-hour lighting, with the HUD hidden. Reproduce it with `FPS_MAP=paldiski FPS_POS=-293,531 FPS_YAW=32 FPS_PITCH=-3 FPS_ATMO=golden FPS_NOHUD=1 FPS_QUALITY=high FPS_SHOT_FRAME=300 FPS_SHOT=/tmp/hero.ppm ./build-release/game`. JPEG compression is the only image processing.

The 50-second silent preview contains five scripted, ten-second sequences rendered by the game: the hilltop, forest-edge movement, soldier stance, golden forest, and aiming/firing. A temporary capture executable used the existing Release object files, fixed 30 Hz capture steps and direct framebuffer output to FFmpeg. The shipped game was not changed for filming. The video is an offline visual demonstration, not multiplayer footage or a performance measurement.
