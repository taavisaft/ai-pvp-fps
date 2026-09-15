# Training tree realism trial — 2026-09-14

Superseded by the [four-type training mix](training-spruce-types-2026-09-15.md).

Training-only mesh and needle texture; Paldiski keeps its original tree mesh,
texture, and impostor. Grass assets and population are unchanged.

Generated original transparent spruce spray saved at
textures/training_spruce_branch.png (1024×1536 RGBA). Built-in imagegen used;
original alpha preserved. CPU load-time edge colour bleed remains in use.

Following the supplied Reforger/DayZ spruce references, the crown now uses 16
tiers of broad boughs with sagging inner spines, slightly lifted outer tips,
inner foliage, and four hanging secondary sprays per bough. A narrow leader
finishes the tapered crown. Near/middle mesh: 5576 triangles; same geometry in
both mesh LODs to avoid ghosting. Saplings retain lower foliage and mature
crowns start at varied heights; the trunk envelope is unchanged. The shared
impostor is a representative shape, not a separate bake per crown variation.
Training impostor baked once at startup, with a 0.62×1.10 capture envelope.
Tree instance streams are reused; no additional per-frame mesh construction.

Woody branch geometry and derivative-filtered bark shading add close detail.
Needle lighting keeps the canopy from flipping black when viewed from below.
Crown width varies by instance through a shared lit/depth deformation function.
Visible and depth passes use matching alpha threshold0.30 for training trees.
Main trunk retains the exact shared six-sided taper, height, and placement;
collision and world generation are unchanged.

Verification on 2026-09-15: both build/game and build-release/game rebuilt.
Tree collision and lobby-ground tests passed, as did git diff --check. Training
shaders loaded successfully and the final crown was visually checked.
Stationary Release medium, 2560×1440, camera (10,39), yaw90/pitch15, 300 warm-up
+ 600 samples: mean6.64ms, p50 6.67ms, p95 8.41ms, max9.17ms.
Single close-view observation, not a before/after improvement claim or proof
of large-forest performance. Preview: build/spruce-preview.png;
log: build/spruce-benchmark.log (both local build artifacts).
The supplied artwork remains the target; this pass still needs user review.

## Exact built-in generation prompt

Use case: photorealistic-natural. Asset type: transparent cutout texture for a spruce tree branch in a realistic 3D game. Create one isolated natural Norway spruce branch spray viewed straight-on, central thin brown woody stem starting at bottom center and reaching toward top center, with irregular finer lateral twigs carrying many individually resolved short fine needles. The silhouette is airy and asymmetrical, with transparent gaps between twigs and groups of needles. Branch longer than wide, entire branch fits with generous clear padding at all edges. Muted natural medium olive forest-green needles, some lighter green growing tips. Flat diffuse overcast illumination from all sides suitable for albedo, no directional lighting or baked deep shadows. True transparent alpha background including holes between needles, not black or white backdrop. No ground, no pot, no scene, no text, no border. Botanical photographic realism, delicate thin needles, not a Christmas tree, not a dense triangular hedge, not waxy broad leaves. Original game-ready botanical artwork.
