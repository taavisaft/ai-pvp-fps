# PvP Shooter

Barebones 3D first-person free-for-all shooter, built from scratch in C++17.
No game engine, no physics library, no networking library — just SDL2, OpenGL 3.3, GLM, and raw UDP sockets.

Up to 16 players drop in and out of a dedicated server, fight around the cover boxes of a small arena, and shoot each other. Move with sprint, jump, and crouch; aim down sights for accurate fire or shoot from the hip with spread. Bullets have travel time and gravity and stop on cover, each life carries 20 rounds, 4 hits kill, and you respawn after 3 seconds at a random spawn point.

![screenshot](screenshot.png)

## Build

Requires CMake 3.20+ and SDL2.

```bash
# macOS
brew install sdl2

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

Starting `./build/game` without an IP gives offline practice mode against a stationary dummy.

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

| Input       | Action                        |
| ----------- | ----------------------------- |
| W A S D     | Move                          |
| Shift       | Sprint                        |
| Space       | Jump                          |
| Left Ctrl   | Crouch                        |
| Mouse       | Look                          |
| Left click  | Shoot (one bullet per click)  |
| Right mouse | Aim down sights (zoom)        |
| B           | Cycle fire mode (semi/burst/auto) |
| R           | Reload                        |
| Tab (hold)  | Scoreboard                    |
| G           | Clear range marks (offline)   |
| C           | Connect (prompts IP on stdin) |
| F           | Toggle wireframe              |
| ESC         | Quit                          |

HP and ammo show on screen, kills appear in the feed top-right, and holding Tab shows the scoreboard.
