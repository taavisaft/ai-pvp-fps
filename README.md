# AI PvP FPS

An online first-person shooter built from scratch in **C++17**, using SDL2, OpenGL and raw UDP. No game engine.

**16-player free-for-all** across Paldiski, a 2×2 km Baltic landscape. Fight with the Uzi and Glock 19, projectile ballistics, and an authoritative dedicated server. Players have **100 HP** and respawn after **3 seconds**. Practice offline or join an online match.

![Golden-hour patrol in Paldiski](screenshot.jpg)

[50-second in-game preview](docs/assets/gameplay-preview.mp4) · Silent, scripted capture from the game.

## Build and play

Requires CMake 3.20+, a C++17 compiler, SDL2 and OpenGL. On macOS: `brew install cmake sdl2`.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/game                         # offline practice
```

For multiplayer, start a server and connect using its IP:

```sh
./build/server                      # UDP port 7777
./build/game 127.0.0.1
```

You can also press **C** in-game to find and join a server. Multi-config builds place binaries under `build/Release/`.

## Controls

| Key | Action |
| --- | --- |
| WASD / Mouse | Move / look |
| Shift / Space | Sprint / jump |
| Left Ctrl | Crouch |
| Q / E | Lean left / right |
| Left / right click | Fire / aim down sights |
| 1 / 2 | Uzi / Glock 19 |
| Scroll wheel | Cycle weapon |
| R | Reload |
| B | Cycle fire mode (Uzi: semi/burst/auto) |
| Tab (hold) | Scoreboard |
| C | Find and join a server |
| M / J | Toggle map / HUD |
| K | Cycle atmosphere |
| V | Toggle third-person view |
| G | Clear bullet marks (offline) |
| H / F | Toggle hitboxes / wireframe (debug) |
| Esc | Quit |

[Roadmap](TODO.md) · [Development notes](docs/development.md) · [Contributing](AGENTS.md)
