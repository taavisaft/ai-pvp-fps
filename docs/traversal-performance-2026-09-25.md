# Native-resolution traversal baseline — 2026-09-25

The next optimization target is meadow rendering near the pond. In matching
native-resolution background route runs, disabling meadow grass reduced average frame time
by 2.76–3.03 ms (19–20%) in the two pond scenarios. Grass remains enabled in normal
play. This change adds measurement infrastructure; it does not change scene quality.

## Reproduce

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
FPS_BENCH_ROUTE=build/traversal-baseline.csv FPS_QUALITY=medium ./build/game > build/traversal-baseline.log 2>&1
FPS_BENCH_ROUTE=build/traversal-no-meadow.csv FPS_QUALITY=medium FPS_NOMEADOW=1 ./build/game > build/traversal-no-meadow.log 2>&1
```

Run sequentially on an otherwise idle machine. On macOS, default desktop
fullscreen uses the native drawable. Other platforms should verify the logged
drawable dimensions before comparing. Keep quality, drawable dimensions,
hardware, route version, and scene assets identical. Earlier windowed pond
benchmarks are not directly comparable to these native-resolution results.
Keep the game focused until it exits. The benchmark raises its window on launch
and flags unfocused or resized samples. A flagged run should not be presented as
an uninterrupted foreground gameplay benchmark.

The route runs offline in the training map with golden-hour lighting. Each of
four scenarios has 180 stationary warmup frames followed by 600 measured frames:

1. Approach the pond over approximately 51 m.
2. Move beside the pond while sweeping the view at up to 180 degrees/second.
3. Traverse approximately 61 m through the northern woodland.
4. Move slowly through woodland while cycling the Kar98's actual scope FOV and overlay twice.

Viewpoints and wind use fixed 1/60-second script steps, so a faster GPU visits
the same sample positions. Wall-clock traversal duration consequently varies
with rendering speed. The camera follows terrain height but bypasses movement
collision; this is a rendering/streaming benchmark, not a movement or multiplayer
correctness test. Warmup excludes startup and inter-scenario teleports. The first
moving traversal of each scenario is measured. Physical gameplay remains idle.

Input cannot change the scripted camera/settings; Escape can interrupt the run.
Do not combine it with fixed-camera, screenshot, third-person or meadow GPU modes.
CSV storage and query objects are reserved before the loop. CSV writing and
sorting occur after measurement. Normal gameplay does not enable GPU queries or
build timing.

## Baseline

Apple M1 Pro, 10 CPU cores, 32 GB RAM; macOS OpenGL 4.1 Metal - 91.7.
Release, medium quality, **3456×2234 drawable**, 1728×1117 logical window,
reported display refresh 120 Hz, swap interval 0. Source base `b1bb451` plus the
current working-tree changes, including the rifle, vegetation updates, reflection
update policy, MSAA removal, and this benchmark.

The final foreground attempt completed all 600 samples of each pond and forest
scenario with focus held. Its frame times were:

| Scenario | Average ms | Median ms | p95 ms | p99 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Pond approach | 14.62 | 14.53 | 18.36 | 19.33 | 20.88 |
| Pond turns | 15.46 | 15.38 | 19.68 | 21.53 | 24.74 |
| Forest traversal | 13.67 | 13.33 | 18.53 | 19.70 | 20.31 |

The scope scenario retained focus for 250/600 samples, then lost it for the final
350. Its aggregate is excluded from this foreground table. A fully focused scope
baseline remains to be captured. Raw records preserve the focus flag so this
cannot silently appear to be a valid complete foreground run.

### Matched background runs used for isolation

The earlier baseline and grass-off runs both had no input focus for all 2,400
measured samples. Their focus state, poses, resolution and rendering settings
match. They provide a controlled diagnostic comparison, but the measured savings
must be reconfirmed with a focused A/B before claiming a foreground improvement.

GPU queries were **off** for this table. Frame time covers the current loop's
simulation/input, rendering and buffer swap. CPU pass timings include submission
and driver waits; they are not GPU execution times.

| Scenario | Average ms | Median ms | p95 ms | p99 ms | Maximum ms | Frames above 16.67 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Pond approach | 14.71 | 14.75 | 18.80 | 20.86 | 22.16 | 140/600 |
| Pond turns | 15.47 | 15.31 | 19.83 | 21.85 | 25.63 | 194/600 |
| Forest traversal | 13.60 | 13.38 | 18.77 | 20.19 | 22.26 | 125/600 |
| Scope cycles | 12.15 | 11.91 | 17.82 | 19.38 | 21.42 | 66/600 |

An earlier identical route run had medians 14.68, 15.51, 13.40 and 11.66 ms.
Its worst pond-turn frame was 32.22 ms. Treat isolated maximums as variable;
the distribution is more useful than declaring an improvement from one spike.
16.67 ms is a reporting threshold for 60 Hz, not a newly agreed performance target.

## Grass isolation

Both runs used the same final benchmark executable and no GPU queries.

| Scenario | Grass on, average ms | Grass off, average ms | Reduction | Grass off, p95 ms |
| --- | ---: | ---: | ---: | ---: |
| Pond approach | 14.71 | 11.95 | 2.76 ms / 18.8% | 15.10 |
| Pond turns | 15.47 | 12.44 | 3.03 ms / 19.6% | 15.67 |
| Forest traversal | 13.60 | 12.29 | 1.31 ms / 9.6% | 16.50 |
| Scope cycles | 12.15 | 11.22 | 0.93 ms / 7.7% | 16.22 |

Terrain and grass generation together averaged 0.02–0.19 ms per frame in the
baseline. The isolated draw-cost reduction is much larger, making meadow geometry,
pixel shading and overlapping blades better next targets than generation alone.
This does not prove generation can never hitch on other routes or hardware.

## GPU diagnostics

`FPS_BENCH_GPU=1` opts into asynchronous GPU timing. It is off by default because
query boundaries substantially perturb this Mac driver. Use these runs for pass
attribution and use untimed runs for performance comparisons.

The profiler checks query counter support. This Mac provides no useful timestamp
counter, so it uses 32-bit elapsed queries for disjoint passes and the gaps between
them. GPU total includes both. The fixed ring keeps the originating sample ID,
reads results only after availability is reported, and skips collection rather
than waiting. Unavailable GPU values are `-1`, not zero. Queries are warmed before
recording. These semantics follow the [Khronos query-object documentation](https://wikis.khronos.org/opengl/Query_Object).

The warmed diagnostic run added approximately 6–8 ms per frame against the
matched background baseline. In that perturbed run, the main world pass averaged
9.96–14.45 ms and dominated; reflections averaged 0.53–0.78 ms in the pond
scenarios, while forest shadows averaged approximately 4 ms. All GPU sample IDs
were received and pass sums were bounded by their corresponding total. These are
background diagnostic observations, not additive estimates of normal untimed
frame cost.

## Files and verification

- Per-frame baseline: `build/traversal-baseline.csv`, with metadata/summary in the matching `.log`.
- Foreground attempt: `build/traversal-foreground.csv` and `.log`; the final scope stage is flagged for focus loss.
- Grass isolation: `build/traversal-no-meadow.csv` and `.log`.
- GPU diagnostic: `build/traversal-gpu-warm.csv` and `.log`.
- CSV records camera pose/ADS, drawable size, focus, frame/render-with-swap times,
  CPU/GPU pass times, tree instances, and terrain/grass build counts and times.
- Release client/server build and all 11 CTest tests pass. The route regression
  checks finite poses, continuous translation/turning, bounded ADS and full scope cycles.
- CSV validation verified all 2,400 rows per run, identical camera samples,
  native drawable dimensions, matching focus state for grass isolation, and
  correct GPU attribution. Focus loss was detected rather than discarded.

Next: profile meadow drawing more narrowly, reduce its measured cost, and rerun
these same routes with visual review. Paldiski traversal, startup hitches,
allocation tracking, 16-player load and Linux/Windows parity remain separate work.
The existing texture-unit warning still needs investigation.
