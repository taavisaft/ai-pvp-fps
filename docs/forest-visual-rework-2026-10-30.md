# Forest visual rework

- Lower, wider broadleaf crowns; lower spruce foliage; shared shadow deformation.
- Reduced double canopy darkening in both mesh shading and impostor baking.
- Greener meadow litter, less yellow grass tint, textured distant fields.
- Spruce cutout: `models/textures/spruce_bough_v2.png`, built-in imagegen. Original retained.
- Review: `FPS_REF=woodland FPS_QUALITY=medium ./build/game`.
- Release build and 20/20 tests passed. Player-height and countryside captures inspected.
- Medium meadow, same settings: 15.29 → 15.25 ms (−0.04 ms, −0.3%); single runs, effectively unchanged.

## Asset prompt

Photorealistic game foliage texture: one dense Norway spruce branch spray isolated on genuinely transparent background. Vertical central woody stem starts exactly at bottom center, branches taper toward top center. Full connected branch with thick overlapping fine green needles on many lateral twigs, roughly triangular outline, width about 65 percent of height. Soft diffuse neutral daylight, natural muted forest green, visible fine needle detail, no cast shadow, no background haze, no blurred vegetation, no text, no border. Entire silhouette inside image with small transparent margin. This is an alpha cutout asset used repeatedly on branch geometry, NOT a tree or landscape. Need genuine alpha transparency between twig groups and around every needle.
