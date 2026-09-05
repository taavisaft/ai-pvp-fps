# Next work — AI PvP FPS

Reviewed against source on 2026-09-05. This is the active roadmap; ordering is a recommendation, not a claim that the work is already implemented. Aim for convincing close-range combat on a stable 16-player server, then expand the world.

## First three tasks

1. **Enforce firing rules on the server.** Make gun cadence correct even when packets arrive late or a client sends an invalid shot count.
2. **Make trees usable cover.** Swept bullets must hit the visible trunk, while gaps and foliage remain shootable.
3. **Build one convincing forest combat area.** A roughly 100×100 m forest edge with varied ground, rocks, understory, one small enterable structure, and purposeful sightlines. Add it in small stages and measure each stage.

Work on input replay and repeatable performance tests alongside these milestones before treating the slice as online-ready.

## Foundation: online correctness and measurement

| Priority | Work | Current evidence / reason | Completion check |
| --- | --- | --- | --- |
| P0 | Server weapon cadence and shot validation | `server_main.cpp::tick` consumes one queued shot per tick; weapon firing intervals are only gated on the client. | Enforce the selected weapon/fire-mode rules with server time; bound queued requests; delayed/duplicated/burst requests cannot accelerate fire or fire stale shots after a swap/respawn. Test ammo and reload behavior too. |
| P0 | Bounded, validated network input | Aim floats are accepted directly; packet receive loops have no work budget; sequence comparisons are not wrap-safe. Some length/version checks already exist. | Reject non-finite/out-of-range values and invalid IDs; validate client ACCEPT/state fields too; bound packet processing and backlog; test floods, truncation, sequence wrap and stale packets. |
| P0 | Fixed-tick inputs, acknowledgments and replay | Client sends input per rendered frame; reconciliation replaces predicted position and smooths the error, with no acknowledged-command history. | Server acknowledges processed command IDs; replay pending commands from complete authoritative movement state (including airborne state). Test starts/stops, jumps, collisions, death/rejoin at differing FPS and 0/100/200 ms RTT with jitter/loss. Effects must not replay twice. |
| P0 | UDP payload budget | Compiled packet sizes: 26-byte input, 581-byte empty snapshot, 1,477-byte snapshot with 64 bullets. All 16 player slots are always included. | Adopt an explicit path-aware payload policy; add size assertions/tests, reduce or split data, and bump protocol version as required. A 1,477-byte payload plus normal IPv4/UDP headers exceeds a 1,500-byte IP MTU. Do not equate a 1,500-byte receive buffer with an MTU-safe payload. |
| P1 | Headless regression and load harness | No CTest/test target is present. Existing lag simulation and the earlier join smoke test do not cover combat correctness. | Reproducible movement/shooting clients at 2/8/16 players; check damage, reload, rewind, disconnect/rejoin, slot reuse, server restart, packet loss/reordering, and all-player firing. Record server tick cost and bandwidth. |
| P1 | Rewind policy under latency | Target history and view-sequence mapping exist, with up to 1.4 seconds of rewind. | Measure moving-target hits and shots around cover; document and bound acceptable rewind for projectile travel, including death/respawn discontinuities. Choose the cap from tests rather than extending it blindly. |
| P1 | Release and GPU performance baseline | Last controlled test was an unoptimized local build; pass timers measure CPU submission/wait. Terrain meshes are lazily built inside drawing. | Repeat all reference views plus a traversal in Release; add nonblocking GPU timing where supported, allocation/build-cost measurements, and 16-player server timing. Record p50/p95/p99, worst hitch, resolution/hardware/settings. Fix the existing texture-unit warning. |

For payload policy, follow [RFC 8085 message-size guidance](https://www.rfc-editor.org/rfc/rfc8085.html#section-3.2). For acknowledged-input replay, see [Valve's latency-compensation design](https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization); adapt it to this project's projectile simulation.

## Realism and combat feel

| Priority | Work | Small first version | Completion check |
| --- | --- | --- | --- |
| P1 | Tree-trunk bullet collision | Share trunk dimensions/placement with rendering; query the existing spatial grid and sweep for the nearest solid hit. Current movement collision is intentionally wider than the rendered tapered trunk. | Fast hits, grazing misses, trunk ends, nearby trees, and a target behind wood behave correctly offline and on the server. Foliage is not invisible solid cover. Measure query cost in a dense stand. |
| P1 | Forest-edge material and cover pass | Break ground repetition with a few scale/material variations, forest litter, rocks, and sparse understory. Reuse existing terrain/material paths first. | Compare bog/forest/ridge views and walking transitions; no conspicuous repetition or LOD popping, no collision mismatch, and no unmeasured frame-time regression. |
| P1 | Better first-person weapon and hands | Improve one existing weapon silhouette, grips, ADS alignment, reload and swap poses before adding guns. | Crosshair/muzzle alignment and visible magazine state agree with gameplay; no near-plane clipping; verify wide/narrow FOV and crouch/lean. |
| P1 | Impact and directional feedback | Surface-specific dirt/wood/metal puffs and audio, a clear damage direction, and distinct headshot feedback. | Bounded pools, no allocations per impact, feedback matches server-confirmed events, and lost cosmetic packets do not affect damage. |
| P1 | One enterable structure and fair spawns | A small cabin/shed using shared static collision, door openings and cover; tune approaches and spawn sightlines in the test area. | Players can enter/peek/shoot through openings without invisible walls; no spawn in solid geometry or an unavoidable firing lane. Two-player playtest before map-wide placement. |
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

These are implemented features, not claims of production readiness. Trees stop movement only; grass blades remain off; Paldiski buildings were reset; no four-hit universal damage rule, fixed ammo-per-life total, or round end exists.

## How to finish each task

Agree on the small completion check above, implement that slice, run relevant proof, and record the result. Proposed targets: server work stays below its 16.67 ms tick budget at 16 players; choose reference hardware before setting a client 120/144 FPS acceptance target. Preserve visible cover across quality tiers. Keep [README.md](README.md) factual and record measurements as dated observations in [LOG.md](LOG.md).
