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
- Multiple weapons with distinct stats (AR, SMG, sniper)
- Per-weapon authored recoil patterns (learnable spray)
- Hitboxes with damage multipliers (headshot / limb)
- Bullet penetration + damage falloff over distance

### Feel / visuals
- Bullet tracers + impact effects (spark/puff, decals)
- Blood/hit feedback + directional damage indicator
- Reload / weapon-swap animations, better muzzle flash

### Content / world
- Proper map design (lanes, cover, sightlines)
- Pickups: health, ammo, weapons
- Grenades (thrown projectile + radius damage)
- AI bots that roam and shoot

### Systems / polish
- Settings menu (sensitivity, FOV, keybinds, volume)
- Main menu / server browser (replace stdin IP prompt)
- Match flow: round timers, score limits, game modes

### Platform
- Windows build + CI
