# Spruce crown density — 2026-09-22

The connected spruce rebuild retained too little inner foliage, especially in the low mesh. Restored overlapping crossed sprays at the elbow and a connected inner shoot near the trunk. The low mesh keeps both inner foliage masses instead of dropping them; outer twigs remain simplified. Wider sprays fill the crown, and spruce alpha cutoff is .22 in both lit and shadow passes to retain finer needles under filtering. Far impostors are regenerated from the fuller mesh at startup.

Low-detail sprays use two rows rather than three, reducing triangles without dropping foliage planes. Full meshes retain curvature. Enlarged the cosmetic billboard/culling envelope from .76 to .84 to contain the wider cards. Collision trunks and world placement remain unchanged. High-crown foliage can now hang down to .25 normalized height; its test still requires a distinct elevated crown relative to full spruce.

Added an eight-direction projected needle-area check: the low mesh must retain at least 70% of full-mesh summed projected foliage area. This checks geometric area, not opaque pixel coverage; in-game inspection separately covered roughly 30 m, 70 m and 140 m. Existing attachment tests pass for both LODs and all profiles, including negative detached-branch and spray cases.

Release build and all 10 tests passed before the final two-row low-mesh optimization; the affected spruce/woodland tests passed again afterward. Local captures: `build/spruce-filled.png`, `build/spruce-dense-distant.png`, `build/spruce-dense-far.png`.

Matched medium 2560×1440 meadow benchmark: sparse connected model 9.42 ms average / 11.78 ms p95; final dense model 10.34 / 13.41 ms. The initial dense three-row low mesh measured 11.38 / 15.65 ms. These are individual CPU submission/wait runs, not isolated GPU timings. Final low spruce triangle counts are 3816, 3464, 3948 and 2144; corresponding full counts are 7262, 6590, 7514 and 4070. The fuller crown has a measurable cost of about 0.9 ms in this scene.
