# Documentation review — 2026-09-05

Scope: all eight project Markdown files outside build/vendor output, checked against current C++/GLSL/CMake and asset inventory. This is documentation work; no game behavior changed. The previous visual inspection and benchmark are in [the dated report](progress-2026-09-05.md).

## Findings and changes

| Document | Stale or misleading content | Resolution |
| --- | --- | --- |
| `TODO.md` | Removed warehouse/field maps marked done; universal four-hit kills; recoil both returning and sticky; server browser listed as missing; snapshots described as truncating player slots. | Replaced with a prioritized roadmap, existing-feature inventory, source evidence and completion checks. Retained useful later ideas. |
| `AGENTS.md` | Two players, listen-server H key, old protocol/structs/shaders, 20 rounds per life, fixed flat arena. | Replaced with current architecture, source ownership, build/verification guidance and original stack/conventions. Preserved starter spec in archive. |
| `CLAUDE.md` | Near-duplicate stale 519-line specification. | Replaced with a pointer to the shared current guide to avoid future drift. |
| `README.md` | Procedural grass claimed active despite the photo change; overconfident no-pop/determinism/lag claims; no current profiling guide. | Corrected feature descriptions, clarified target-vs-tested platforms, added Release and fixed-view profiling commands, and links to the roadmap. |
| `LOG.md` | Empty. | Added index of dated inspection/review records. |
| `docs/progress-2026-09-05.md` | Dated measurements and next-step observations, not a living spec. | Retained unchanged as historical evidence; current prioritization lives in TODO. |
| `textures/README.md` | Claimed the whole directory could be empty; provider label could be mistaken for a verified license. | Distinguished optional materials from required vegetation cutouts, and recorded missing provenance/copy behavior. |
| `textures/environment/README.md` | Described four atlases absent from the directory and proposed paths as though assets were available. | Replaced with current file/use inventory; preserved old tile plans in archive. |

## Important verified facts

- `game.h`: 16 players, 256 simulated bullets, 60 Hz physics, 20 Hz state broadcast.
- `protocol.h`: protocol v3, all player slots in every state. A compiled size probe returned input=26 bytes, player=35 bytes, empty state=581 bytes, full state=1477 bytes, maximum impact packet=418 bytes.
- `server_main.cpp::tick`: shot-count backlog is consumed once per tick without consulting weapon firing intervals. Reload/ammo checks exist; that is not cadence enforcement.
- `main.cpp`: input packets are sent at render cadence; authoritative position corrections are smoothed without command acknowledgment/replay.
- `physics.cpp::updateBullets`: box, terrain and posed-player sweeps exist; no tree-trunk bullet sweep. `collideTrees` does handle player movement.
- `map.h`: Paldiski and lobby are the current map IDs. Paldiski generation creates spawns, not buildings; joining uses slot-selected spawns and death uses a random point from the current selection logic.
- `camera.cpp::recoverRecoil`: recoil is baked into aim after firing, so it stays where the spray ends despite stale comments about recovery elsewhere.
- `perf.cpp`: grass disabled for low/medium/high. `main.cpp`: screenshot at frame 60, benchmark samples after frame 300; `FPS_REF=all` resolves to the first camera only.
- `CMakeLists.txt`: separate headless server/client; no automated test target; shaders copied every build, textures/sounds only after a client link. No `.github` workflow directory was found.
- `terrain_render.cpp` and `mesh.cpp`: lazy terrain construction allocates vectors while rendering, so the no-loop-allocation convention is not fully met today.
- Environment directory contains the README, forest-floor JPEG and material atlas PNG. Only the forest-floor JPEG is referenced in current source.

Validation: checked Markdown local links, archive preservation, whitespace, implementation references and the compiled packet-size probe. No game rebuild was needed for Markdown-only edits. Windows/Linux runtime, fire-rate exploit behavior, full-load networking, and asset licenses were not newly tested or certified by this review.

## Suggested next session

Start with server shot cadence and focused headless regression coverage, then tree-trunk bullet collision. Build the forest combat area in measured visual increments; keep input replay and the UDP budget on the foundation track before a serious internet playtest. See [TODO.md](../TODO.md) for acceptance criteria and later ideas.
