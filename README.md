# PvP Shooter

Barebones 3D first-person free-for-all shooter, built from scratch in C++17.
No game engine, no physics library, no networking library — just SDL2, OpenGL 3.3, GLM, and raw UDP sockets.

Up to 16 players drop in and out of a dedicated server and fight across **Paldiski** — a 500×500 m Estonian coastal town. Move with sprint, jump, and crouch; aim down sights for accurate fire or shoot from the hip with movement spread, and respawn after 3 seconds at a random spawn point.

Two weapons — a 9mm **Uzi** SMG and a **Glock 19** pistol — fire real projectiles with true muzzle velocity (~375–400 m/s), bullet drop, air drag, and distance damage falloff. Collision is swept per bullet, so fast rounds stop dead on cover instead of tunnelling through it. Recoil is sticky: the gun climbs as you spray and stays where it ended — you pull it back down yourself. Switch weapons with the number keys or the scroll wheel — each gun keeps its own magazine and reserve, so swapping isn't a free reload. The mag reloads automatically when it runs dry, or reload early with R. Lean around cover with Q/E to peek without exposing your body.

The map is **Paldiski**: the Baltic Sea on the west behind a wavy shoreline you can wade into (waist-deep, then it walls you off), a sandy beach rising to a grassy bank — steeper and higher on the north stretch, a nod to the Pakri cliffs — a row of Soviet apartment slabs on flattened ground near the shore, and forested hills rolling inland to the east. Everything is generated deterministically from integer-hash noise that runs identically on the server and every client, so the whole town needs no assets and nothing to sync. The yards are dressed with instanced props — dumpsters, parked cars, crate stacks, and lamp posts — and thousands of trees clump into forests on the hills. Underfoot, a dense ring of instanced 3D grass clumps surrounds the camera out to ~45 m — thousands of wind-swaying blade cards placed by a deterministic hash grid (nothing stored, nothing synced), lit with the terrain's own normal so they melt into the ground, dither-fading into the flat grass texture at the ring's edge. Every kind of world clutter (trees, props, buildings) is view-frustum culled and distance-LODed each frame — far buildings drop their window facades for plain concrete boxes and small props fade out — so the scene stays dense without the framerate paying for what's off-screen or too far to read. Offline you practice on the same map against a respawning dummy; the map registry (`FPS_MAP`, one name today: `paldiski`) keeps the door open for future maps.

![screenshot](screenshot.png)

## Build

Requires CMake 3.20+ and SDL2.

```bash
# macOS
brew install cmake sdl2

# Ubuntu/Debian
sudo apt install cmake build-essential libsdl2-dev libgl1-mesa-dev

# Windows
# Nope, I won't touch that even with a 20-inch stick.

# then
cmake -B build
cmake --build build
```

Produces two binaries: `build/game` (client) and `build/server` (dedicated server).

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

Starting `./build/game` with no IP drops you into the **lobby** — a small flat test
range (target wall with a bullseye, practice cover, a respawning dummy) where you can
warm up and test things before joining a match. It's client-only and not a real map.
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

- **Lag compensation:** the server rewinds player positions to the moment you fired, so strafing a target and firing on the crosshair still registers despite the delay.
- **Interpolation buffer:** remote players render a couple of snapshots behind the newest packet, so jitter and the occasional dropped packet stay smooth instead of stuttering or snapping.

Without these variables (or on a LAN) there's effectively no delay, so behavior is unchanged.

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
