# PvP Shooter

Barebones 3D first-person 1v1 shooter, built from scratch in C++17.
No game engine, no physics library, no networking library — just SDL2, OpenGL 3.3, GLM, and raw UDP sockets.

Two players connect to a small dedicated server, run around a flat arena, and shoot each other. Bullets have travel time and gravity. 4 hits win the match.

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

# terminals 2 and 3 — start a client for each player
./build/game 127.0.0.1
```

For two machines, run the server anywhere reachable and pass its IP to each client.

Starting `./build/game` without an IP gives offline practice mode against a stationary dummy.

## Controls

| Input      | Action                        |
| ---------- | ----------------------------- |
| W A S D    | Move                          |
| Mouse      | Look                          |
| Left click | Shoot (one bullet per click)  |
| C          | Connect (prompts IP on stdin) |
| F          | Toggle wireframe              |
| ESC        | Quit                          |

HP and win/lose are printed to the terminal.
