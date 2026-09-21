# Pond reflection continuity — 2026-09-21

Continued the reference-landscape work against the existing September 17 landscape
and its uncommitted shared-vegetation port. Those changes were preserved.

The pond renders its mirror every second frame. Previously it sampled that cached
image with the current camera's screen coordinates, so camera movement displaced
reflections relative to their trees. The water now projects its world positions
through the saved reflection capture matrix. Newly exposed capture edges blend
toward the existing sky fallback rather than stretching border texels.

The cache refreshes immediately after invalidation, a translation over 2 metres,
a direction change with dot product below .98, or a projection change (ADS/FOV or
aspect ratio). Eligibility checks run before the reuse decision, invalidating the
cache when the pond is offscreen or too distant, the eye is below water, or the
active map changes.
Normal movement retains the every-second-frame render schedule. No new textures,
render targets, or per-frame heap allocations were introduced.

Both Release targets built. All 10 CTests passed, including localhost UDP and new
headless reflection tests covering reflection-plane projection, mirrored tree-tip
projection, camera movement during reuse, invalidation, turns, teleports and zoom.
The live shader compiled and the stationary meadow capture was visually inspected.
Interactive motion still needs a human visual check; the automated proof tests the
projection math and refresh policy.

Local 2560×1440, medium, meadow reference, 300 warm-up + 600 samples: updated frame
mean 9.67 ms, median 9.59, p95 12.81, maximum 19.73 (~103 FPS from mean). A preceding
run measured mean 10.22 ms, but the weapon changed from Uzi to Glock in the updated
run due to desktop input, so these are **not a controlled speedup comparison**.
These are CPU-side whole-frame intervals, not GPU pass times or traversal proof.
Capture: `build/landscape-resumed.png`. Logs were recorded under `/tmp/fps-resume-*`.

The scene is materially closer in composition than the original training pad,
but distant ground detail, fine grass aliasing at medium, and foliage silhouettes
remain visible differences from the photograph. This patch addresses continuity
in motion; it does not claim a photographic match.
