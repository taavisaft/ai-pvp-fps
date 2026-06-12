# PvP Shooter

Barebones 3D first-person free-for-all shooter, built from scratch in C++17.
No game engine, no physics library, no networking library — just SDL2, OpenGL 3.3, GLM, and raw UDP sockets.

Up to 16 players drop in and out of a dedicated server, fight around the cover boxes of a small arena, and shoot each other. Bullets have travel time and gravity and stop on cover, each life carries 20 rounds, 4 hits kill, and you respawn after 3 seconds at a random spawn point.

![screenshot](screenshot.png)

## Build

Requires CMake 3.20+ and SDL2.

```bash
# macOS
brew install sdl2

# Ubuntu/Debian
sudo apt install libsdl2-dev

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

## Controls

| Input      | Action                        |
| ---------- | ----------------------------- |
| W A S D    | Move                          |
| Mouse      | Look                          |
| Left click | Shoot (one bullet per click)  |
| Tab (hold) | Scoreboard                    |
| C          | Connect (prompts IP on stdin) |
| F          | Toggle wireframe              |
| ESC        | Quit                          |

HP and ammo show on screen, kills appear in the feed top-right, and holding Tab shows the scoreboard.
