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

### Testing lag compensation

The server rewinds player positions to the moment a shooter fired, so shots register even with high ping. To try this on one machine, set `FPS_LAG` (one-way latency in milliseconds) on the client to simulate a laggy connection:

```bash
# terminal 1
./build/server

# terminal 2 — client with a simulated 150 ms one-way delay (300 ms round trip)
FPS_LAG=150 ./build/game 127.0.0.1
```

Strafe a target and fire on the crosshair: hits land where you aimed despite the delay. Without `FPS_LAG` (or on a LAN) there's effectively no rewind, so behavior is unchanged.

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
| Tab (hold)  | Scoreboard                    |
| C           | Connect (prompts IP on stdin) |
| F           | Toggle wireframe              |
| ESC         | Quit                          |

HP and ammo show on screen, kills appear in the feed top-right, and holding Tab shows the scoreboard.
