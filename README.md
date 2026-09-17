# AI PvP FPS

[![Download for macOS](https://img.shields.io/badge/Download_for_macOS-Apple_Silicon-2d4938?style=for-the-badge&logo=apple&logoColor=white)](https://github.com/taavisaft/ai-pvp-fps/releases/download/v0.1.0-alpha.1/ai-pvp-fps-0.1.0-macos-arm64.zip)

**Alpha 1 · Apple Silicon Macs only.** Extract the ZIP and open **AI PvP FPS.app**. If macOS blocks this unnotarized alpha, use **System Settings → Privacy & Security → Open Anyway** after attempting to open it. [Release notes](https://github.com/taavisaft/ai-pvp-fps/releases/tag/v0.1.0-alpha.1).

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

Test the current grass and terrain material across Paldiski offline with
`FPS_MAP=paldiski ./build/game`. Grass streams around the camera and does not cast
shadows. `FPS_NOMEADOW=1` disables the new grass for performance comparisons.

Keila (WIP): 1×1 km of real Keila, Estonia, centred on Keila Kultuurikeskus. Terrain, building shells and roads baked from Maa- ja Ruumiamet open data. North is −Z.

```sh
FPS_MAP=keila ./build/game           # offline
FPS_MAP=keila ./build/server         # online
python3 tools/keila/bake.py          # rebake; see tools/keila/README.md
```

Offline practice spawns in a 2×2 km training landscape: shooting range at the origin, meadow, pond and wooded hills to the north.

```sh
FPS_REF=meadow ./build/game          # fixed cameras: meadow|pond|canopy|range (training), shore|bog|forest|ridge|golden (Paldiski)
FPS_MSAA=4 ./build/game              # 0|2|4; default 0 on low/medium, 4 on high (~+3 ms at 1440p)
FPS_NOREFLECT=1 ./build/game         # pond mirror off, for comparisons
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
