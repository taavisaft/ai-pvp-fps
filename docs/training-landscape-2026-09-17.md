# Training landscape — 2026-09-17

Target: `~/Desktop/Desktop/Trees Grass/HSWkWlPWEAAXRsN.jpg`. World revision `0x20260917`.

## Frame time

Release, medium, 2560×1440, M1 Pro, `FPS_REF=<cam> FPS_QUALITY=medium FPS_NOHUD=1 FPS_BENCH=1`. CPU-side frame ms, avg of 600.

| camera | inherited | final |
|---|---|---|
| meadow | 12.33 | 10.14 |
| pond | 11.16 | 9.18 |
| canopy | 12.06 | 11.43 |
| range | 10.11 | 8.91 |

Cameras moved with the spawn/pond; scene went 21k → 51k trees. Budget: ≤ 11 meadow/pond, ≤ 12.5 canopy.

## What cost what

- `discard` anywhere in a fragment shader disables hidden-surface removal on Apple GPUs. Grass got its own discard-free `meadow.frag`: 4.6 → 2.2 ms.
- Per-plant values (growth colour, cloud shadow, haze colour) computed in `veg.vert`, not per blade fragment.
- Seed heads/leaves exist on ~12 % of plants; drawn from a second small instance buffer instead of collapsing vertices on the rest: −0.4 ms.
- Trees: species bucketed once at scatter (was O(7·N) per pass), real low-LOD meshes, shrubs always low mesh, 2×2 PCF on foliage.
- An extra render target costs ~0.8 ms even when empty (macOS GL). Pond mirror renders every second frame with coarse terrain and low tree meshes.
- 4× MSAA: +3.3 ms, 2×: +1.5 ms, independent of scene. Off by default below high.
- Outer terrain at 2 m instead of 4 m: +0.7 ms for ≤ 9 cm difference. Kept 4 m.

## Look

- Pond basin is a broad concave lawn, not a pit; spawn (0,136) sees water with ≥ 1 m clearance over grass (tested).
- Broadleaf crowns start at 0.11 of height with a skirt ring; shrubs are small tree instances.
- Forest mask sharpened into stands; 40 m species stands; conifer share rises with distance; copses in open ground.
- `landscape_map.cpp` bakes 1024² RG: canopy cover and sun-direction shadow streaks. Terrain and impostors sample it; rebaked on atmosphere change.
- Lobby golden preset: sun behind-left, fog 1250 m, haze cools away from the sun (`atmosphere.glsl`, `hazeCool`). Panorama rotated to the sun.
- Terrain far field mixes toward grass-tip colour at grazing angles; hides the 50 m grass edge. Smooth-normal triplanar weights removed contour stripes.

## Open

- Pond mostly mirrors the far bank from eye height; brighter water needs a higher vantage or lower far trees.
- No AA at medium. Post-process AA needs an offscreen scene target (~0.8 ms by itself).
- Tree collision exists for shrubs (thin trunks).
- `veg_depth.vert` still carries unused grass-shadow code.
