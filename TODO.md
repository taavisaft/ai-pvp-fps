# TODO

A simple running list of what's done and what's next.

## Done

- Dedicated headless server + client over raw UDP (port 7777)
- Drop-in free-for-all, up to 16 players, spawn on a circle, respawn after 3s
- Variable-length state protocol (truncated to active players/bullets)
- Arena with cover boxes: collision, push-out, bullets stop on cover/ground
- Movement: WASD, sprint, jump, crouch
- Aim down sights (ADS) with FOV zoom
- Bullets with travel time + gravity
- Hit detection (AABB), HP, 4 hits to kill, ammo per life, reload
- Fire modes: semi / burst / auto (B to cycle)
- PUBG-style recoil: precise when still, climbing spray, recovers when you stop
- Movement/stance accuracy: walk/sprint/jump bloom, crouch tighter, ADS steadies
- First-shot recoil kick
- HUD: crosshair, HP/ammo bars, hit flash, death overlay + respawn countdown
- Scoreboard (Tab), kill feed, FPS readout
- World-anchored hit markers (shows where your shot landed)
- First-person weapon viewmodel + muzzle flash
- Audio: shoot, footsteps, jump, death, respawn, reload, dry-fire
- Depth cues: flat shading, ground grid, distance fog, blob shadows
- Offline practice vs a respawning dummy
- Offline shooting range: target wall + persistent impact marks (G clears)
- Default training mode on launch; connect/switch servers anytime
- In-game server-IP entry overlay (C key) — no more stdin blocking
- Three maps: training arena (offline), warehouse yard, and a 1 km² open field —
  server picks via FPS_MAP=field; clients auto-detect it from the join handshake
  (mapId in AcceptPacket), so connecting alone is enough
- Large field map: shared procedural heightfield terrain (deterministic noise, same
  on server + client) with scattered cover snapped onto the surface; physics,
  spawns, bullets, and shadows all follow the terrain (arena maps stay flat)
- Lean (Q/E): peek around cover — camera roll + upper-body arc, with a
  server-authoritative shot origin so you fire from the peek
- Weapon swap preserves each gun's magazine + reserve (no free reload); magazine
  auto-reloads when it runs dry
- Realistic projectile ballistics: swept-segment collision, real muzzle velocity
  (~400 m/s) + bullet drop, no tunneling at any speed
- Per-weapon stats (WeaponDef table); distance damage falloff + air drag
- Two weapons — Uzi (SMG) + Glock 19 (semi pistol), per-player + server-authoritative,
  switch with 1/2 keys or scroll (weaponId in InputPacket); distinct viewmodels
- Articulated character model (head/torso/arms/legs) with a speed-driven walk cycle
  (crouch/jump poses), client-side cosmetic
- Regional hitboxes with damage multipliers — 2x head, 1.5x neck, 1x torso, 0.8x legs,
  plus a forward arms/hands box (1x) that tracks the gun-hold pose (yaw-projected,
  raised at ADS); lag-comp-safe (snapshot carries pos + crouched + yaw + ads)
- Sticky recoil: spray climbs and stays, you pull it back down
- Lag-compensated hit rewind (server rewinds targets to when you fired)
- Client-side prediction + prediction-error smoothing
- Netcode snapshot playout buffer (smooth under jitter and packet loss)
- Network sim for testing: FPS_LAG, FPS_JITTER, FPS_LOSS
- Uncapped frame rate (VSync off)
- Debug frame dump (FPS_SHOT)

## To do

### Netcode / core (harden the base first)
- Connection lifecycle: server-restart recovery, clean rejoin / slot reuse
- Server-side input validation + rate limiting (client is trusted for aim)
- Deterministic headless tests for physics + netcode
- Collision edge cases (no getting stuck in / tunneling through boxes)

### Gunplay
- More weapons (AR, sniper) — WeaponDef table + per-player weaponId already in place
- Weapon pickups / loadout select (1/2 hotkeys swap between two fixed weapons)
- Deterministic projectile netcode: send each shot as an (origin, dir, speed) event,
  clients simulate the trajectory — fast bullets are near-invisible to other players
  at 20 Hz position streaming (needed for proper tracers on big maps)
- Per-weapon authored recoil patterns (learnable spray); per-weapon recoil feel
  (both weapons still share the global recoil constants)
- Headshot feedback (distinct hit marker / sound) — server doesn't yet send hit region
- Animation-following limb hitboxes: arms now track the gun-hold pose (yaw + ads in the
  snapshot); legs/feet still don't follow the walk stride
- Bullet penetration through thin cover

### Feel / visuals
- Bullet tracers + impact effects (spark/puff, decals)
- Blood/hit feedback + directional damage indicator
- Reload / weapon-swap animations, better muzzle flash

### Content / world
- More maps + map rotation/voting; tune warehouse lanes & sightlines
- Enterable structures (hollow buildings w/ doorways — engine is solid boxes only)
- Pickups: health, ammo, weapons
- Grenades (thrown projectile + radius damage)
- AI bots that roam and shoot

### Systems / polish
- Settings menu (sensitivity, FOV, keybinds, volume)
- Main menu / server browser (upgrade the in-game IP overlay; remember last IP)
- Match flow: round timers, score limits, game modes

### Platform
- Windows build + CI
