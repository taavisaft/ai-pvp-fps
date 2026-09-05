# AGENTS.md — AI PvP FPS

Current guide, reviewed against source on 2026-09-05. Build a realistic, performant online first-person shooter from scratch: DAYZ look, PUBG feel, Tarkov influence.

## Constraints and conventions

- C++17, SDL2 for platform/input/audio, OpenGL 3.3 Core, GLSL 330, GLM 0.9.9.8, raw UDP. No game engine, physics library, or networking library.
- Target macOS 12+, Ubuntu 22.04+, Windows 10+ (MSVC 2019+ or MinGW). macOS was exercised locally; Linux/Windows parity still needs verification.
- Dedicated authoritative server must stay headless: no SDL or OpenGL dependency.
- No exceptions; use `bool` return codes for error paths.
- No heap allocation in the game loop; pre-allocate arrays/buffers at startup. Existing lazy terrain/vegetation build paths need work toward this rule; do not treat them as proof of compliance.
- Store angles in degrees; convert for trig/GLM operations. Y-up, right-handed coordinates.
- Cache uniform locations during shader setup, never query them every frame.
- Keep each `.cpp` under 300 lines. Several inherited files exceed this limit; split touched responsibilities when appropriate rather than growing monoliths or doing unrelated cleanup.
- Use `#pragma once` in headers and fixed-width types in wire packets.
- Preserve user changes. Keep rendering and shared gameplay collision consistent.

## Current implementation

- `game` client and `server` executable; 16-player drop-in FFA, 100 HP, three-second respawn. No rounds or victory condition.
- 60 Hz authoritative simulation and 20 Hz snapshots. Raw UDP defaults to port 7777; server accepts `FPS_PORT`.
- Protocol v4 and world revision live in `src/protocol.h`. Snapshots contain all 16 player slots plus an occupied-slot mask and up to 63 bullet records; only the bullet tail is truncated. Simulation has 256 bullet slots.
- Paldiski is the online map, approximately 2×2 km (±1024 m). Offline launch starts in the separate training lobby. Old warehouse/field maps are removed.
- Shared terrain and tree scatter drive server/client placement. Tree trunks block movement and swept bullets; bullet collision matches the visible taper, while movement keeps a wider cylinder. Paldiski has a first forestry shelter and timber-cover layout near (325, 75); the lobby has cover and a hunting stand.
- Sprint, jump, crouch, lean, ADS, Uzi/Glock, preserved per-weapon ammo, manual/automatic reload, sticky recoil, swept projectile collision, drag/falloff, posed regional hitboxes, and target rewind exist.
- Client prediction currently smooths position corrections; it does not replay acknowledged/unacknowledged input history. Shot cadence, bounded queues and state epochs are enforced by `server_fire.cpp`; broader packet validation/rate limits remain unfinished.
- Segmented Blender-authored tactical soldier and classic Uzi use normals/material vertex data, local camouflage, and reduced meshes beyond 15 m. The Uzi is shared by first-/third-person views; gameplay hitboxes are unchanged. Editable source and export instructions: `tools/soldier/README.md`. No skinned deformation or magazine/reload animation yet.
- Textured terrain, instanced vegetation/LOD/impostors, shadow map, sky/clouds, atmosphere presets, water, HUD, minimap, scoreboard, impact decals, audio, and cosmetic ragdolls exist. Grass blades are disabled in all quality tiers.
- C opens a host prompt and server browser scanning eight ports; it is not a blocking stdin prompt. H toggles hitboxes, not hosting.

## Source ownership

| Area | Files |
| --- | --- |
| Build / client loop | `CMakeLists.txt`, `src/main.cpp` |
| Shared gameplay / weapons | `src/game.h`, `src/weapon.h`, `src/physics.h/.cpp`, `src/projectiles.cpp` |
| World / collision placement | `src/map.h`, `src/forest_site.h/.cpp`, `src/terrain.h`, `src/tree_scatter.h/.cpp`, `src/tree_collision.h/.cpp`, `src/spatial.h` |
| Server / transport / wire data | `src/server_main.cpp`, `src/server_fire.h/.cpp`, `src/server_rewind.h/.cpp`, `src/network.h/.cpp`, `src/net_common.h/.cpp`, `src/platform.h`, `src/protocol.h` |
| Rendering / terrain / plants | `src/renderer.h/.cpp`, `src/terrain_render.h/.cpp`, `src/vegetation.h/.cpp`, `src/veg_mesh.cpp`, `shaders/` |
| Meshes / textures / materials | `src/mesh.h/.cpp`, `src/material.h/.cpp`, `src/texture.h/.cpp`, `src/gl_loader.h/.cpp` |
| Characters / weapon pose | `src/playerpose.h`, `src/skeleton.h`, `src/player_mesh.h`, `src/player_visual.h/.cpp`, `src/uzi_mesh.h`, `tools/soldier/`, `src/weapon_visual.h`, `src/ragdoll.h` |
| Input / UI / audio / profiling | `src/input.h/.cpp`, `src/camera.h/.cpp`, `src/hud.h/.cpp`, `src/font.h/.cpp`, `src/lobby.h/.cpp`, `src/audio.h/.cpp`, `src/perf.h/.cpp` |

## Build and verification

```sh
cmake -B build && cmake --build build
./build/server
./build/game 127.0.0.1
```

For performance comparisons use an optimized build consistently:

```sh
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
FPS_REF=forest FPS_QUALITY=medium FPS_BENCH=1 ./build-release/game
```

Multi-config generators put executables in their configuration subdirectory. See [README.md](README.md) for controls, dependencies, and debug commands.

- Build both targets for shared gameplay/protocol changes. Bump the protocol version for wire-layout changes and world revision for deterministic world-generation changes.
- Add focused headless regression proof for physics/network changes. Run `ctest --test-dir build --output-on-failure`; firing/client-epoch tests are headless, and the UDP integration test is included when Python3 is found. It launches its own temporary localhost server.
- Compare identical quality, camera, drawable resolution, build type, and hardware for rendering work. Benchmark output is CPU submission/wait timing, not GPU pass time.
- Exercise malformed/reordered/lost input and 16-client load before claiming online robustness. Existing latency simulation is a debug aid, not proof.
- Do not reimplement finished starter stages or replace current shaders/packet structs with the historical examples.

## Documentation ownership

[README.md](README.md): running/features. [TODO.md](TODO.md): prioritized work and completion criteria. [LOG.md](LOG.md): links to dated observations. [docs/archive/](docs/archive/): historical references, not active requirements.
