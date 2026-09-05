# AI PvP FPS

3D first-person free-for-all shooter, built from scratch in C++17.

[Next work](TODO.md) · [Contributor guide](AGENTS.md) · [Development records](LOG.md)
No game engine, no physics library, no networking library — just SDL2, OpenGL 3.3, GLM, and raw UDP sockets.

Up to 16 players drop in and out of a dedicated server and fight across **Paldiski** — a 2×2 km Baltic taiga. Move with sprint, jump, and crouch; aim down sights for accurate fire or shoot from the hip with movement spread, and respawn after 3 seconds at a random spawn point.

Two weapons — a 9mm **Uzi** SMG and a **Glock 19** pistol — fire real projectiles with true muzzle velocity (~375–400 m/s), bullet drop, air drag, and distance damage falloff. Collision is swept per bullet, so fast rounds stop dead on cover instead of tunnelling through it. Recoil is sticky: the gun climbs as you spray and stays where it ended — you pull it back down yourself. Switch weapons with the number keys or the scroll wheel — each gun keeps its own magazine and reserve, so swapping isn't a free reload. The mag reloads automatically when it runs dry, or reload early with R. Lean around cover with Q/E to peek without exposing your body.

The map is **Paldiski**: approximately 2×2 km of procedural Baltic-inspired terrain (±1024 m), with a shoreline, rivers, bog meadows, ridges, and a mountain backdrop. Shared terrain and scatter code supplies placement to the client and server; cross-platform determinism still needs automated verification. About 41,000 spruces are scattered in the current map. The renderer uses instancing, distance tiers and baked tree impostors with dithered transitions, plus terrain chunk LOD and quality presets.

Tree trunks block movement but **bullets still pass through them**. Grass blades are disabled in all quality tiers; the shipped ground image supplies the terrain's grass base, with procedural fallback when missing. Trees and bushes use separate cutout textures. Paldiski buildings and props were reset for a terrain-first rebuild; the offline lobby retains cover, the hunting stand and a practice dummy. See [TODO.md](TODO.md) for the planned forest combat area.

![screenshot](screenshot.png)

## Build

Requires CMake 3.20+ and SDL2.

```bash
# macOS
brew install cmake sdl2

# Ubuntu/Debian
sudo apt install cmake build-essential libsdl2-dev libgl1-mesa-dev

# Windows
# SDL2 and an OpenGL-capable toolchain are required.
# Platform branches exist; Windows build/runtime parity is not yet verified.

# then
cmake -B build
cmake --build build
```

Produces two binaries: `build/game` (client) and `build/server` (dedicated server).
Use an optimized build for performance comparisons:

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
```

For a multi-config generator, binaries are under the selected configuration directory
(e.g. `build-release/Release/`). macOS has been tested locally; Linux and Windows
remain intended targets requiring build/runtime verification.

## Run

```bash
# terminal 1 — start the server (UDP port 7777)
./build/server

# one terminal per player (up to 16)
./build/game 127.0.0.1
```

For multiple machines, run the server anywhere reachable and pass its IP to each client.

Clients auto-detect the server's map on connect (it's sent in the join handshake).
There is one map today — **Paldiski** — but the `FPS_MAP` registry stays wired so
future maps just add a name. Several servers can share one host on distinct ports
with `FPS_PORT` (default `7777`); the client lobby scans `7777`–`7784` on the host
you enter.

Starting `./build/game` with no IP drops you into the **lobby** — a small test
range (target wall with a bullseye, practice cover, a respawning dummy wearing the
Blender-built body model all players share — vest, backpack, boots. The dummy
mirrors your moves from its spot without turning — aim pitch, lean, crouch, ADS,
weapon swaps, jumps, and a walk-in-place when you walk — so you can circle it and
study the pose from any angle) where you can
warm up and test things before joining a match. Behind the firing line rises a
terraced hill (~16 m, flat ledges at set heights) and past the wall's north end a
smooth knoll — climb one to practice downhill shots, or shoot up the knoll's face to
read each weapon's bullet drop at range. It's client-only and not a real map.
Press **C** to type a host address, then **Enter** to scan it for running games: a
server browser lists every game found with its map name, player count, and ping. Use
**Up/Down** to pick one and **Enter** to join (**Esc** cancels); losing the connection
returns you to the lobby. `FPS_MAP=paldiski ./build/game` starts offline on the real
map instead (debug).

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
not GPU execution. See the [dated benchmark](docs/progress-2026-09-05.md) for the
HUD improvement, including its limited test scope.

The current wire format is protocol v3: all 16 player slots plus up to 64 bullet
records, while the server simulates up to 256 bullets. Server cadence validation,
input replay, packet sizing and 16-client load tests are outstanding work, not
finished guarantees of internet robustness. Full controls above describe the
current prototype; the [archived starter spec](docs/archive/original-game-spec.md)
is historical.
