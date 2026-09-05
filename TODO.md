# Next work — AI PvP FPS

Reviewed against source on 2026-09-05. This is the active roadmap; ordering is a recommendation, not a claim that the work is already implemented. Aim for convincing close-range combat on a stable 16-player server, then expand the world.

## First three tasks

1. **Completed: server firing rules.** Per-mode cooldowns, bounded/expiring requests, firing-state epochs, and regression tests are implemented. See [the implementation record](docs/server-firing-2026-09-05.md).
2. **Completed: tree cover.** Swept bullets hit the visible tapered trunk; gaps and foliage remain shootable. Shared simulation tests and a dense-stand microbenchmark pass. See [the implementation record](docs/tree-cover-2026-09-05.md).
3. **In progress: forest combat area.** First stage: an enterable forestry shelter, eight timber stacks and clear approaches near (325, 75), with shared collision and movement tests. Next: varied ground, natural rock cover, understory refinement and a two-player sightline/spawn playtest. See [record](docs/forest-site-2026-09-05.md).

Work on input replay and repeatable performance tests alongside these milestones before treating the slice as online-ready.

## Foundation: online correctness and measurement

| Priority | Work | Current evidence / reason | Completion check |
| --- | --- | --- | --- |
| Done | Server weapon cadence and shot validation | `server_fire.cpp` enforces per-mode intervals, four pending requests and 350 ms expiry. Epoch changes discard requests across weapon/mode/reload/life transitions. | Headless server/client tests and real UDP regression cover rate limits, queue/sequence behavior, state changes, ammo/reload and actual spawn counters. Human trigger-edge detection and distinct lost-shot aim samples still belong to future shot-event work. |
| P0 | Bounded, validated network input | Server aim/weapon/mode validation and wrap-safe input/shot sequences now exist. Receive loops still lack work budgets, and client snapshot/control validation and sequence-wrap handling remain incomplete. | Reject non-finite/out-of-range values and invalid IDs; validate client ACCEPT/state fields too; bound packet processing and backlog; test floods, truncation, sequence wrap and stale packets. |
| P0 | Fixed-tick inputs, acknowledgments and replay | Client sends input per rendered frame; reconciliation replaces predicted position and smooths the error, with no acknowledged-command history. | Server acknowledges processed command IDs; replay pending commands from complete authoritative movement state (including airborne state). Test starts/stops, jumps, collisions, death/rejoin at differing FPS and 0/100/200 ms RTT with jitter/loss. Effects must not replay twice. |
| P0 | UDP payload budget | Protocol v4 sizes: 31-byte input, 586-byte empty snapshot, 1,468-byte snapshot with 63 bullets. A compile-time 1,472-byte ceiling now protects standard IPv4 MTU sizing; all 16 player slots remain included. | Adopt an explicit path-aware payload policy; add size assertions/tests, reduce or split data, and bump protocol version as required. The standard IPv4 bound is fixed; smaller paths/tunnels and bandwidth efficiency still need work. Do not equate a 1,500-byte receive buffer with a universally safe payload. |
| P1 | Headless regression and load harness | CTest now has focused firing, client epoch, and optional Python UDP regressions. A broad movement/combat/16-client load harness is still missing. | Reproducible movement/shooting clients at 2/8/16 players; check damage, reload, rewind, disconnect/rejoin, slot reuse, server restart, packet loss/reordering, and all-player firing. Record server tick cost and bandwidth. |
| P1 | Rewind policy under latency | Target history and view-sequence mapping exist, with up to 1.4 seconds of rewind. | Measure moving-target hits and shots around cover; document and bound acceptable rewind for projectile travel, including death/respawn discontinuities. Choose the cap from tests rather than extending it blindly. |
| P1 | Release and GPU performance baseline | A controlled Release comparison exists for the soldier/Uzi pass; pass timers measure CPU submission/wait. Terrain meshes are lazily built inside drawing. | Repeat all reference views plus a traversal in Release; add nonblocking GPU timing where supported, allocation/build-cost measurements, and 16-player server timing. Record p50/p95/p99, worst hitch, resolution/hardware/settings. Fix the existing texture-unit warning. |

For payload policy, follow [RFC 8085 message-size guidance](https://www.rfc-editor.org/rfc/rfc8085.html#section-3.2). For acknowledged-input replay, see [Valve's latency-compensation design](https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization); adapt it to this project's projectile simulation.

## Realism and combat feel

| Priority | Work | Small first version | Completion check |
| --- | --- | --- | --- |
| Done | Tree-trunk bullet collision | Shared six-sided taper and placement; grid-based nearest swept hit. Movement retains its wider cylinder. | Fast/grazing/end/boundary and protected-target tests pass, including server rewind. Dense-stand queries: 0.226 ms per 256 in local Release; see [record](docs/tree-cover-2026-09-05.md). |
| P1 | Forest-edge material and cover pass | Break ground repetition with a few scale/material variations, forest litter, rocks, and sparse understory. Reuse existing terrain/material paths first. | Compare bog/forest/ridge views and walking transitions; no conspicuous repetition or LOD popping, no collision mismatch, and no unmeasured frame-time regression. |
| P1 | Better first-person weapon and hands | Soldier and classic Uzi geometry/materials, full/reduced meshes, first-person glove/sleeve and iron-sight alignment added on 2026-09-05. Next: articulated support/firing hands, continuous skinned body deformation, reload/magazine and swap poses; improve surface maps before adding guns. | Crosshair/muzzle alignment and visible magazine state agree with gameplay; no near-plane clipping; verify wide/narrow FOV and crouch/lean. |
| P1 | Impact and directional feedback | Surface-specific dirt/wood/metal puffs and audio, a clear damage direction, and distinct headshot feedback. | Bounded pools, no allocations per impact, feedback matches server-confirmed events, and lost cosmetic packets do not affect damage. |
| P1 | One enterable structure and fair spawns | First shelter and cover layout implemented with shared collision; tune local multiplayer spawns and sightlines next. | Players can enter/peek/shoot through openings without invisible walls; no spawn in solid geometry or an unavoidable firing lane. Two-player playtest before map-wide placement. |
| P2 | Shot events and selective tracers | Represent projectile birth with stable IDs/timing and velocity; reconstruct cosmetics while keeping server impact/damage authority. | At 20 Hz snapshots and under loss, bullets remain visually understandable without duplicate tracers or per-frame position bandwidth for every cosmetic projectile. Not every round needs a bright tracer. |
| P2 | Surface-aware movement audio | Distinguish ground/wood/metal steps and landings; improve distance and obstruction cues. | Another player can locate movement without hearing through every wall at full strength; audio work stays bounded. |
| P2 | Per-weapon recoil and motion | Separate weapon tuning, modest sway/bob and animation; preserve sticky recoil and clear aim response. | Uzi/Glock feel distinct without making camera movement nauseating; behavior remains consistent across frame rates. |
| P2 | Small grass experiment | One near-field patch with density/LOD and alpha-overdraw limits. Grass blades are currently disabled in every quality preset. | Measure GPU/frame cost before enabling wider coverage; low quality must not remove gameplay-critical concealment or cover. |

## Usability and portability

- [ ] Settings menu: sensitivity, FOV, volume, keybinds, quality, window mode and VSync; save locally. Current controls/settings are mostly hard-coded or environment variables.
- [ ] Extend the existing server browser with last host, explicit full/incompatible/timeout messages and clean reconnect UX. Do not rebuild discovery: host entry and eight-port scan already exist.
- [ ] Add Linux/Windows build and smoke CI; audit Winsock socket-handle types and deterministic terrain behavior across compilers. Current source has platform branches, not verified platform parity.
- [ ] Package shaders/textures/sounds reliably. Shader copies run every build; texture/sound copies currently depend on relinking the game.
- [ ] Make licensed asset provenance explicit: exact source, author, license and modifications. The current ground image attribution is only a provider label; do not infer its license.

## Later, after the combat area plays well

- Rifle and longer-range gunplay; loadout selection/pickups.
- Thin-cover penetration and ricochet with clear material rules and server tests.
- Grenades with cover-aware blast tests.
- Roaming combat bots (separate from deterministic load-test clients).
- Round timers/score limits and then additional maps/modes if playtesting supports them.
- Armor/inventory/extraction-style systems only after choosing whether that complexity fits this game's core loop.

## Already implemented — do not schedule again

Dedicated server; 16-player FFA; three-second respawn; Paldiski plus offline lobby; sprint/jump/crouch/lean/ADS; Uzi and Glock with per-weapon ammunition/reload; sticky recoil; projectile travel/gravity/drag/falloff; swept box/terrain/posed-body hits; target rewind; snapshot interpolation; basic prediction/error smoothing; host-entry/server discovery; minimap/scoreboard/kill feed; decals; audio; character posing/ragdolls; terrain chunks; instanced trees/bushes and impostors; shadow map/clouds/fog/atmosphere; quality presets; reference captures and corrected frame benchmarks.

These are implemented features, not claims of production readiness. Trees stop movement and bullets at the trunk; grass blades remain off; Paldiski has one forestry shelter; no four-hit universal damage rule, fixed ammo-per-life total, or round end exists.

## How to finish each task

Agree on the small completion check above, implement that slice, run relevant proof, and record the result. Proposed targets: server work stays below its 16.67 ms tick budget at 16 players; choose reference hardware before setting a client 120/144 FPS acceptance target. Preserve visible cover across quality tiers. Keep [README.md](README.md) factual and record measurements as dated observations in [LOG.md](LOG.md).
