# Full lobby grass experiment — 2026-09-13

Latest commit inspected: `0da8434` (texture/shadow binding fixes and diagnostics).
The existing dense meadow now covers the 120 × 120 m waiting area by default,
with worn paths and box clearances preserved. 467,595 plants occupy 576 static
5 m tiles. Existing density thinning, shadows and 38 m draw range are retained.
Instance buffers and rank storage are prepared on lobby initialization; drawing
adds no allocations. Paldiski and gameplay collision are unchanged.

`FPS_MEADOW_PATCH=1` restores the original 20 × 20 m patch and surrounding sparse
grass. `FPS_NOMEADOW=1` still restores sparse grass alone.

## Local comparison

Release, medium, 2560 × 1440 drawable, position (5,27), yaw 145°, pitch −10°,
default atmosphere, same local hardware. GPU timers disabled. Each completed run
discards 300 frames and measures 600. Two expanded runs exited early and were
excluded. Single completed run per configuration: indicative, not a multi-view
or sustained thermal benchmark.

| Coverage | Mean frame | Median | p95 | Max | Approx. FPS from mean |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original patch | 8.13 ms | 8.07 ms | 10.92 ms | 44.22 ms | 123 |
| Full lobby | 10.25 ms | 10.16 ms | 13.63 ms | 46.91 ms | 98 |

Full coverage adds 2.12 ms (26%) to mean frame time in this view.
CPU submission/wait means: shadow 0.18 → 0.62 ms, world 0.14 → 0.21 ms.
These are not GPU pass timings. The screenshot was visually checked; nearby
coverage is dense, with visible thinning toward the existing range boundary.

```sh
FPS_QUALITY=medium FPS_POS=5,27 FPS_YAW=145 FPS_PITCH=-10 FPS_BENCH=1 ./build-release/game
FPS_MEADOW_PATCH=1 FPS_QUALITY=medium FPS_POS=5,27 FPS_YAW=145 FPS_PITCH=-10 FPS_BENCH=1 ./build-release/game
```

Both Release targets built. All seven regression tests passed (the UDP test
required localhost socket access outside the sandbox). `git diff --check` passed.

## Follow-up: continuous distance thinning

Removed the dense meadow's 38 m cull and collective height fade. Grass now
continues across the lobby. Stable ranked clumps thin with distance: approximately
27% are eligible at 28 m, 5.6% at 60 m and a 2.5% floor farther away. A narrow
rank transition reduces individual departing clumps' width while retained clumps
keep full height. Distant width compensation is capped at 1.5×. Lit and shadow
shaders use the same curve; CPU prefix submission avoids processing excluded
clumps. Sparse fallback grass retains its previous behavior.

Same fixed-camera Release setup: mean 11.82 ms, median 12.07 ms, p95 15.62 ms,
max 21.00 ms (approximately 85 FPS). Compared with the earlier full-lobby cutoff
run, this adds 1.57 ms; these single runs are indicative. Shadow CPU submission/
wait averaged 1.13 ms and world 0.26 ms, not GPU timings. A final-frame screenshot
confirmed grass on the previously bare distant slopes. Walking transitions still
need user visual review. Both normal and Release builds completed; the updated
lobby-ground density regression passed and the diff passed whitespace checks.

## Follow-up: simpler clumps and coverage compensation

A twelve-blade, 36-triangle mesh now replaces the detailed 192-triangle clump
through a per-instance staggered 18–32 m transition. Both meshes reuse the
existing sorted instance buffers. Tile tests omit detailed draws beyond the
transition and omit far draws before it. The transition changes horizontal
coverage, preserving height. Width compensation follows inverse square root of
density, capped at 4×. The far mesh is static; nearby wind diminishes through the
transition. Both shadow and lit passes use the same geometry rules, and tile
bounds were expanded to contain wider clumps. All new mesh/VAO setup is performed
at lobby initialization and destroyed with the other meadow resources.

The lobby terrain blends toward a textured olive understorey from 16–48 m,
preserving the existing worn-path mask. This terrain treatment also applies in
sparse comparison mode. No grass cards or new textures were introduced.

Final fixed-camera Release/medium/2560×1440 run: mean 12.28 ms (81 FPS), median
12.39 ms, p95 15.80 ms, max 19.56 ms. Previous thinning-only mean was 11.82 ms:
this improves visible coverage, but does not demonstrate a performance gain.
The overlapping transition draws and broader screen coverage offset geometry
savings. CPU submission/wait means were shadow 1.32 ms and world 0.32 ms;
these are not GPU timings. A first broader-leaf prototype was visually rejected
and narrowed before this final run. Final screenshot was inspected at the same
camera; moving transitions still need playtesting. Both build/game and
build-release/game were rebuilt; lobby_ground regression and diff checks passed.

## Follow-up: grass-matched ground pattern

Replaced the lobby's pale turf/litter meadow blend with static crossed strand
noise in the same root/tip palette used by meadow_mesh.cpp. Screen derivatives
filter fine strands toward their mean before they become undersampled. Dirt
paths and slope-based rock retain their materials. Removed the old isolated
20 m patch darkening and the partial distance tint, so the under-grass colour
is consistent across the lobby. This is a procedural shader pattern, not a new
bitmap texture, and also appears when grass geometry is disabled.

Both build shader copies updated. Shader compilation and the fixed-camera view
were checked in-game. Release/medium/2560×1440: mean 11.88 ms (~84 FPS), median
12.14 ms, p95 15.64 ms, max 18.48 ms. Single-run variation prevents claiming a
speedup over the previous 12.28 ms result. Screenshot inspection confirmed darker
gaps matching the grass more closely. Diff whitespace check passed.

## Follow-up: faster thinning and unified distance colour

With the darker ground carrying coverage, reduced the density curve's distance
scale from 12 to 8 m and its far floor from 2.5% to 0.4%. Eligible clumps are now
13.8% at 28 m and 2.5% at 60 m. Width compensation is capped at 1.5× instead of
4× to avoid oversized spots. CPU and both geometry shaders use matching values.
Root darkening, dry tint and brightness variation blend toward a common olive
colour over 16–44 m; ground patch and macro contrast reduce over 16–50 m. Nearby
plants retain their original colours. Tree shadows and worn paths remain.

Normal and Release builds updated; density regression and whitespace checks
passed. Inspected fixed-camera screenshot: distant coverage is more uniform.
Same Release benchmark: mean 10.76 ms (~93 FPS), median 10.20 ms, p95 16.66 ms,
max 22.04 ms. Earlier mean was 11.88 ms, but p95 was lower at 15.64 ms; this
single run supports a lower average, not improved tail latency. CPU submission/
wait: shadow 1.01 ms, world .27 ms. Movement still requires playtesting.

## 2026-09-14: textured clump prototype

Original generated three-column RGBA atlas in textures/meadow_atlas.png; no game
assets copied. 65% green, 30% olive and 5% seedhead variants. Nearby geometry is
three bent planes (18 triangles); far geometry is two static planes (4 triangles).
Candidates reduced from 900 to 225 per tile. The existing distance density and
staggered transition remain. Visible/depth shaders use identical atlas UVs and
alpha cutoff. Packed terrain normals let far card lighting approach terrain
lighting; shaders/grass_surface.glsl supplies the same world-space colour field
to terrain and grass. Shader loading now expands bounded relative includes.
The photograph retains more colour detail nearby and blends toward this field
with distance. The original ribbon geometry remains available with
FPS_GRASS_RIBBONS=1. Texture-only rebuilds now refresh both launch directories.

Controlled Release/medium, 2560×1440, position (5,27), yaw 145, pitch -10,
default atmosphere, GPU timer disabled. Each run discards 300 warm-up frames
and measures 600; runs performed sequentially on the same machine.

| Mode | Mean ms | Median ms | p95 ms | Max ms | FPS from mean |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ribbon comparison | 9.83 | 9.71 | 12.53 | 15.53 | 102 |
| Textured clumps | 4.78 | 4.69 | 8.67 | 15.19 | 209 |

This measures the combined mesh, instance-count and material change, not the
isolated benefit of texture cards. CPU submission/wait shadow means .95 → .75 ms,
world .24 → .22 ms. These are not GPU timings. Single-view results are indicative;
movement/zoom aliasing, clump repetition and other viewpoints need playtesting.
The rejected first atlas looked like hedges; the selected sparse atlas has more
transparent space, but this remains a visual prototype rather than a DayZ match.

Both builds and the focused lobby regression passed. Runtime shader compilation,
cutout rendering and final screenshot were checked. Source/build atlas hashes
matched. No gameplay collision or wire changes; no commit created.

## 2026-09-14: replace flat cutout silhouettes

The card prototype was rejected visually. A narrow-strip cutout trial still
showed ragged atlas contours in close-up, so the current mesh gives each blade
its own tapered geometric silhouette. This is a newly shaped mesh, not a rollback
to the original meadow. 32 independently curved/leaning blades near, nine simpler
blades far; varied height, width, root position and normal across each leaf.
Texture intensity contributes only subtle surface detail. Visible and shadow
passes both use the tapered geometry without the former card alpha test.
Population increased to 450 candidates per tile. Colours vary along and between
blades, with restrained dry stems; distant shared terrain colour remains.

The shape revision benchmark (before final palette-only adjustment) averaged
10.06 ms, median 9.69 ms, p95 15.66 ms, max 108.02 ms at the same fixed reference
camera/settings. The final palette was checked in a separate screenshot, not
re-benchmarked. Both builds and the lobby regression passed. The appearance still
needs user review and should not be represented as matching DayZ quality.

## Training growth patches — 2026-09-14

Training only: a deterministic, smoothly warped growth field now drives terrain
colour, grass population and blade height together. Pale dry soil/litter retains
only 1.5% of candidate tufts at the minimum; dark growth reaches full density
and taller blades. Intermediate areas transition continuously. CPU placement
and GLSL colouring use matching field functions. Existing Paldiski materials
and population remain unchanged by this pass; grass shadows remain disabled.

This reuses the current textures and tapered blades, with procedural material
variation; no new bitmap texture was generated. Both build/game and
build-release/game rebuilt, focused lobby_ground test passed, and the Release
training preview at (5,27), yaw145/pitch-10 rendered successfully. Screenshot:
/tmp/fps-growth.png. Pale patches visibly correspond to sparse, short grass;
the exposed soil still needs visual refinement. No new benchmark claim.
