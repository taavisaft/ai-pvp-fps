# AGENTS.md — Cross-Platform 3D PvP Shooter

Barebones 3D first-person PvP game. Two players, over internet, built from scratch.
No game engine. No physics library. No networking library.

## Inspirations for this game

- DAYZ look
- PUBG feel
- Tarkov spice

## Implemented Deviations from the Spec Below

- **Dedicated server** instead of listen server: separate `server` executable (headless,
  no SDL/GL) runs authoritative simulation on port 7777; players run `game` clients.
  `H` key dropped. Connect: `./build/game <ip>` or `C` key (IP prompt on stdin).
- **Drop-in FFA, 16 players** instead of 1v1 rounds: `MAX_PLAYERS = 16`, players spawn
  on a circle (radius 15) as they join. No match end — die → respawn after 3 s at your
  spawn with full HP. Each life has **20 ammo** (`AMMO_PER_LIFE`); refilled on respawn.
- **Protocol v2**: `StatePacket` is variable-length — header + `usedMask` (occupied
  slots) + 16 `PlayerNetState` + up to 64 `BulletNetState` (each carries pool index for
  interpolation + owner). Sent truncated via `statePacketSize(count)`. GameState has no
  gameOver/winner.
- Extra files: `src/game.h` (shared structs/constants), `src/gl_loader.h/.cpp` (GL 3.3
  function loading on Linux/Windows via SDL_GL_GetProcAddress; macOS uses the framework),
  `src/net_common.h/.cpp` (shared UDP helpers), `src/server_main.cpp` (server entry).
- **Arena obstacles** (`src/map.h`, shared server+client): static cover boxes — center
  pillar, axis walls, low crates (shootable over), outer pillars. Players push out of
  boxes (XZ least-overlap, in `movePlayer`), bullets stop on boxes and the ground.
  Players clamped to ±45 on X/Z. Obstacles ≥1 m thick so bullets can't tunnel.
  Death respawn picks a random spawn point; joining uses your slot's point.
- HUD: crosshair, HP/ammo bars + numbers, hit flash, death overlay with respawn
  countdown, Tab scoreboard (kills/deaths, sorted), kill feed. Text via embedded 5x7
  bitmap font (`src/font.h/.cpp`, atlas texture + `shaders/text.vert/.frag`), HUD
  logic in `src/hud.h/.cpp`. Protocol carries per-player kills/deaths (uint8).
- Depth cues, all in `basic.frag` (no assets, no extra passes): flat shading from
  `dFdx/dFdy` normals, 1 m grid on ground-level faces, distance fog toward sky color
  (`lit`/`eyePos` uniforms; HUD sets `lit=0`). Blob shadows = flattened dark cubes
  under players/bullets.
- Debug: `FPS_SHOT=<path.ppm> ./build/game` dumps a frame ~1 s after start.
- Offline practice mode: client starts vs a stationary respawning dummy until connected;
  ammo auto-refills offline.
- Kill/death/join events printed to stdout.

---

## Stack

| Layer        | Choice          | Notes                                     |
| ------------ | --------------- | ----------------------------------------- |
| Language     | C++17           |                                           |
| Window/Input | SDL2            | Platform abstraction only — not an engine |
| Rendering    | OpenGL 3.3 Core | Cross-platform ceiling; macOS caps at 4.1 |
| Shaders      | GLSL 330 core   | Vertex + fragment only                    |
| Math         | GLM 0.9.9.8     | Header-only, via CMake FetchContent       |
| Networking   | Raw UDP sockets | POSIX on macOS/Linux, Winsock on Windows  |
| Build        | CMake 3.20+     |                                           |

## Platforms

- macOS 12+
- Ubuntu 22.04+
- Windows 10+ (MSVC 2019+ or MinGW)

---

## File Structure

```
game/
├── CMakeLists.txt
├── AGENTS.md
├── src/
│   ├── main.cpp           # entry point, game loop
│   ├── platform.h         # Winsock vs POSIX ifdefs — copy from this file
│   ├── renderer.h/.cpp    # OpenGL init, shader setup, draw calls
│   ├── shader.h/.cpp      # compile + link GLSL programs, error logging
│   ├── mesh.h/.cpp        # VAO/VBO/EBO upload and draw
│   ├── camera.h/.cpp      # view matrix, yaw/pitch mouse look
│   ├── physics.h/.cpp     # bullet trajectory, AABB collision
│   ├── input.h/.cpp       # per-frame keyboard+mouse snapshot
│   ├── network.h/.cpp     # UDP socket send/recv, non-blocking
│   └── protocol.h         # all packed packet structs
└── shaders/
    ├── basic.vert
    └── basic.frag
```

---

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(pvp_shooter CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

include(FetchContent)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8)
FetchContent_MakeAvailable(glm)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(game ${SOURCES})
target_include_directories(game PRIVATE src)
target_link_libraries(game PRIVATE SDL2::SDL2 OpenGL::GL glm::glm)

if(WIN32)
    target_link_libraries(game PRIVATE ws2_32)
    target_compile_definitions(game PRIVATE _WIN32_WINNT=0x0601)
endif()

# Copy shaders next to the binary
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:game>/shaders)
```

## Build Commands

```bash
# macOS
brew install sdl2
cmake -B build && cmake --build build
./build/game

# Ubuntu/Debian
sudo apt install libsdl2-dev
cmake -B build && cmake --build build
./build/game

# Windows (PowerShell, after installing SDL2 via vcpkg)
vcpkg install sdl2:x64-windows
cmake -B build -DCMAKE_TOOLCHAIN_FILE="[vcpkg root]/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug
```

---

## platform.h ← copy this exactly

```cpp
#pragma once

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socklen_t = int;
  inline void platformSocketInit()    { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); }
  inline void platformSocketCleanup() { WSACleanup(); }
  inline void setNonBlocking(int fd)  { u_long m = 1; ioctlsocket(fd, FIONBIO, &m); }
  inline void closeSocket(int fd)     { closesocket(fd); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <unistd.h>
  inline void platformSocketInit()    {}
  inline void platformSocketCleanup() {}
  inline void setNonBlocking(int fd)  { fcntl(fd, F_SETFL, O_NONBLOCK); }
  inline void closeSocket(int fd)     { close(fd); }
#endif
```

---

## Shaders ← copy these exactly

```glsl
// shaders/basic.vert
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
void main() {
    gl_Position = proj * view * model * vec4(aPos, 1.0);
}
```

```glsl
// shaders/basic.frag
#version 330 core
uniform vec3 color;
out vec4 fragColor;
void main() {
    fragColor = vec4(color, 1.0);
}
```

---

## OpenGL Setup (renderer.cpp)

```cpp
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

// After context creation:
glEnable(GL_DEPTH_TEST);
glEnable(GL_CULL_FACE);
glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
```

Vertex layout: location 0 = `vec3` position only. Stride = `3 * sizeof(float)`.

---

## Core Data Structures

```cpp
// All in a shared header, e.g. game.h

constexpr int   MAX_BULLETS  = 16;
constexpr int   PLAYER_HP    = 100;
constexpr float BULLET_SPEED = 50.0f;   // m/s
constexpr float BULLET_TTL   = 3.0f;    // seconds
constexpr float MOVE_SPEED   = 5.0f;    // m/s
constexpr float GRAVITY      = 9.8f;    // m/s²
constexpr float BULLET_DMG   = 25;      // HP per hit
constexpr int   NET_HZ       = 20;      // state sync rate
constexpr int   PHYS_HZ      = 60;      // physics tick rate
constexpr int   UDP_PORT     = 7777;

struct Player {
    glm::vec3 pos   = {0, 0, 0};  // feet, Y always 0
    float     yaw   = 0.0f;       // degrees
    int       hp    = PLAYER_HP;
    bool      alive = true;
};

struct Bullet {
    glm::vec3 pos;
    glm::vec3 vel;
    float     lifetime = 0.0f;
    int       ownerID  = -1;      // 0 or 1
    bool      active   = false;
};

struct GameState {
    Player players[2];
    Bullet bullets[MAX_BULLETS];
    int    bulletCount = 0;
    bool   gameOver    = false;
    int    winnerID    = -1;       // -1 = in progress
};

struct InputState {
    bool  w, a, s, d;
    bool  shoot;                   // true on press, not hold
    float yaw;
    float pitch;
};
```

---

## Camera (camera.h/.cpp)

```cpp
// Position is player.pos + vec3(0, 1.7f, 0)  (eye height)
// yaw and pitch in degrees; update from SDL_MOUSEMOTION xrel/yrel * sensitivity (0.1f)
// Clamp pitch to [-89, 89]

glm::vec3 front = glm::normalize(glm::vec3(
    cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
    sin(glm::radians(pitch)),
    sin(glm::radians(yaw)) * cos(glm::radians(pitch))
));

glm::mat4 view = glm::lookAt(eyePos, eyePos + front, {0,1,0});
glm::mat4 proj = glm::perspective(glm::radians(75.0f), aspect, 0.1f, 500.0f);
```

Mouse capture on start: `SDL_SetRelativeMouseMode(SDL_TRUE)`.

---

## Physics (physics.cpp)

Fixed timestep: `dt = 1.0f / PHYS_HZ` (i.e. 1/60).

**Bullet each tick:**

```cpp
bullet.vel.y   -= GRAVITY * dt;
bullet.pos     += bullet.vel * dt;
bullet.lifetime -= dt;
if (bullet.lifetime <= 0.0f) bullet.active = false;
```

**Bullet spawn (on shoot):**

```cpp
bullet.pos      = cameraEyePos;
bullet.vel      = cameraFront * BULLET_SPEED;
bullet.lifetime = BULLET_TTL;
bullet.ownerID  = localPlayerID;
bullet.active   = true;
```

**Hit detection — AABB:**
Player AABB half-extents: `(0.4f, 1.0f, 0.4f)` centered at `player.pos + vec3(0, 1.0f, 0)`.

```cpp
// Returns true if point p is inside player's AABB
bool aabbHit(glm::vec3 p, glm::vec3 playerPos) {
    glm::vec3 center = playerPos + glm::vec3(0, 1.0f, 0);
    glm::vec3 half   = {0.4f, 1.0f, 0.4f};
    return abs(p.x - center.x) < half.x &&
           abs(p.y - center.y) < half.y &&
           abs(p.z - center.z) < half.z;
}
```

Check hit on each bullet against each opponent each tick. On hit: `opponent.hp -= BULLET_DMG`, deactivate bullet.

**Player movement:**

```cpp
glm::vec3 forward = glm::normalize(glm::vec3(cos(glm::radians(yaw)), 0, sin(glm::radians(yaw))));
glm::vec3 right   = glm::normalize(glm::cross(forward, {0,1,0}));
if (input.w) player.pos += forward * MOVE_SPEED * dt;
if (input.s) player.pos -= forward * MOVE_SPEED * dt;
if (input.a) player.pos -= right   * MOVE_SPEED * dt;
if (input.d) player.pos += right   * MOVE_SPEED * dt;
player.pos.y = 0.0f; // lock to ground
```

---

## What to Render

| Object        | Geometry         | Color                    |
| ------------- | ---------------- | ------------------------ |
| Ground        | 100×100 quad y=0 | `vec3(0.30, 0.50, 0.30)` |
| Remote player | Box 1×2×1        | `vec3(0.80, 0.30, 0.20)` |
| Own bullets   | Cube 0.1³        | `vec3(1.00, 0.90, 0.20)` |
| Enemy bullets | Cube 0.1³        | `vec3(1.00, 0.40, 0.10)` |

Local player is NOT rendered (first-person camera). Use `glm::translate` + `glm::scale` for model matrix per object.

---

## Input Map

| Key / Button | Action                             |
| ------------ | ---------------------------------- |
| W A S D      | Move forward / left / back / right |
| Mouse move   | Look (yaw/pitch)                   |
| Left click   | Shoot (one bullet per click)       |
| H            | Host server on port 7777           |
| C            | Connect — prompt IP on stdin       |
| ESC          | Quit                               |
| F            | Toggle wireframe (debug)           |

---

## Networking Protocol (protocol.h)

```cpp
#pragma once
#include <cstdint>

enum PacketType : uint8_t {
    PKT_HELLO  = 1,   // client → server: request to join
    PKT_ACCEPT = 2,   // server → client: player ID assigned
    PKT_INPUT  = 3,   // client → server: this frame's input
    PKT_STATE  = 4,   // server → client: full authoritative state
    PKT_BYE    = 6,   // either direction: clean disconnect
};

#pragma pack(push, 1)

struct HelloPacket  { PacketType type; };   // PKT_HELLO
struct AcceptPacket { PacketType type; uint8_t playerID; };  // 0 or 1

struct InputPacket {
    PacketType type;   // PKT_INPUT
    uint32_t   seq;    // monotonically increasing, for drop detection
    uint8_t    keys;   // bitmask: W=1 A=2 S=4 D=8 SHOOT=16
    float      yaw;
    float      pitch;
};

struct BulletNetState { float x, y, z; };

struct StatePacket {
    PacketType    type;          // PKT_STATE
    uint32_t      seq;
    float         p0x, p0y, p0z, p0yaw; int32_t p0hp;
    float         p1x, p1y, p1z, p1yaw; int32_t p1hp;
    uint8_t       bulletCount;
    BulletNetState bullets[16];
    uint8_t       gameOver;
    int8_t        winnerID;     // -1, 0, or 1
};

struct ByePacket { PacketType type; };

#pragma pack(pop)
```

**Architecture:**

- Server runs authoritative physics (GameState lives on server).
- Client sends `InputPacket` every frame.
- Server sends `StatePacket` at NET_HZ (20 Hz).
- Client interpolates received positions for smooth rendering.
- Server only accepts 2 connections; rejects further `HELLO` packets.

**Socket setup:**

```cpp
// Server
int fd = socket(AF_INET, SOCK_DGRAM, 0);
setNonBlocking(fd);
sockaddr_in addr{}; addr.sin_family = AF_INET;
addr.sin_port = htons(UDP_PORT); addr.sin_addr.s_addr = INADDR_ANY;
bind(fd, (sockaddr*)&addr, sizeof(addr));

// Client
int fd = socket(AF_INET, SOCK_DGRAM, 0);
setNonBlocking(fd);
// store server sockaddr_in; sendto / recvfrom use it each packet
```

---

## Game Loop (main.cpp)

```cpp
int main() {
    platformSocketInit();
    SDL_Init(SDL_INIT_VIDEO);
    // create window, GL context, renderer, meshes, shaders

    float    accumulator = 0.0f;
    const float FIXED_DT = 1.0f / PHYS_HZ;
    Uint64   last        = SDL_GetPerformanceCounter();
    float    netTimer    = 0.0f;
    bool     running     = true;

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = (float)(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (dt > 0.05f) dt = 0.05f;   // cap at 50ms to avoid spiral

        // 1. Poll SDL events → update InputState, handle H/C/ESC/F
        pollInput(running);

        // 2. Recv all pending UDP packets (non-blocking)
        pollNetwork();

        // 3. Fixed-step physics
        accumulator += dt;
        while (accumulator >= FIXED_DT) {
            updatePhysics(FIXED_DT);   // movement, bullet integrate, AABB
            accumulator -= FIXED_DT;
        }

        // 4. Send network update at NET_HZ
        netTimer += dt;
        if (netTimer >= 1.0f / NET_HZ) {
            sendNetworkUpdate();
            netTimer = 0.0f;
        }

        // 5. Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        render();
        SDL_GL_SwapWindow(window);
    }

    platformSocketCleanup();
    SDL_Quit();
    return 0;
}
```

---

## Build Order

Implement and test each stage before starting the next.

1. **Window + triangle** — SDL2 init, GL context, compile shaders, draw one colored triangle.
2. **Cube + camera** — mesh.cpp with cube VAO/VBO/EBO, GLM matrices, WASD + mouse look.
3. **Ground + movement** — ground quad, player moves on XZ, Y locked to 0.
4. **Bullets** — shoot on click, trajectory with gravity, bullet cubes rendered.
5. **Hit detection** — AABB hit, HP deduction, win condition, on-screen HP text (or just stdout).
6. **Networking** — host/connect, sync positions, server-authoritative physics, StatePacket broadcast.

Test stage 1–5 with both players on the same machine (WASD for P1, arrows for P2) before doing networking.

---

## Conventions

- No exceptions. Use `bool` return codes for error paths.
- No heap allocation in the game loop. Pre-allocate all arrays at startup.
- All angles stored in degrees, converted to radians only at GLM call sites.
- Coordinate system: Y-up, right-handed (standard OpenGL).
- Never call `glGetUniformLocation` every frame — cache locations at shader compile time.
- Keep each `.cpp` under 300 lines. Split into smaller files if it grows.
- `#pragma once` everywhere (no include guards).
- All packet structs use fixed-width types (`uint8_t`, `int32_t`, `float`).
