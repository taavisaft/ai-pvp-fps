# Tree proportions and loading-area palette — 2026-09-22

Spruce bough wood and the main needle cards now follow the same midpoint bend. Solid twig tips taper inside the needle silhouette. Removed the repeated bare lower spoke stubs; spruce now uses the shared spatial wind deformation already used by broadleaves in both lit and depth passes.

Broadleaf spray counts scale with supporting shoot length, with modest size reduction on short shoots. Reduced low-LOD card enlargement from 1.90 to 1.65. Connected branch construction and the collision trunk remain intact. Changes apply to both maps.

Loading-area golden lighting retains its sun direction but reduces sun intensity, exposure (1.10 to 0.92) and saturation (1.06 to 0.90), aiming for the subdued Paldiski palette. Paldiski atmosphere settings are unchanged.

## Validation

Release build succeeded; all 10 CTests passed. Spruce/woodland tests passed again after the final twig-tip taper adjustment. Visually inspected meadow, close spruce, close oak and Paldiski forest captures. Existing tests cover finite geometry, bounds, determinism, collision trunks and broadleaf attachments.

Matched local meadow benchmark: medium, 2560×1440, same reference camera and hardware, 300 warmup / 600 measured frames. Before: average 10.16 ms, p95 13.46 ms. After: average 9.37 ms, p95 12.96 ms. One pair of CPU submission/wait measurements, not GPU timings or a general speedup guarantee. Measured before final full-LOD spruce tip taper; meadow benchmark had zero full-LOD trees, and the taper does not change triangle counts.

Full mesh triangle counts: ash 7494→5564, birch 5504→4194, oak 7494→5614. No additional render passes or textures.

Captures are local ignored build artifacts: `build/meadow-before-tuning.png`, `build/meadow-after-tuning.png`, `build/spruce-after-tuning.png`, `build/oak-after-tuning.png`, `build/paldiski-after-tuning.png`.

Foliage still uses textured cards; extreme closeups can reveal their planes. This pass addresses unsupported spikes and overpacked short shoots, not a replacement with individually modeled leaves/needles.
