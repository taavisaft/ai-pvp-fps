# Woodland floor atlas

Generated with the built-in imagegen tool on 2026-10-11 for this project.
This is generated albedo artwork, not a measured scan. Layout: top-left moss,
top-right needles, bottom-left broadleaf litter, bottom-right sandy organic soil.
The leaf tile is a general visual litter reference, not a species identification.

Used only in the offline lobby through the existing forest texture binding.
Paldiski retains its original forest photograph and grass behavior. `forest_floor.glsl` uses
mirrored, warped world coordinates and quadrant-safe mip insets. The CPU grass
mask mirrors the shader habitat boundary. Existing tree placement, collision
and terrain height are unchanged.

Release, medium quality, native 3456×2234 drawable, fixed `FPS_REF=canopy`:
before 18.64 ms average / 23.71 ms p95; final 16.67 ms / 20.92 ms.
The average difference is −1.97 ms (−10.6%). These are single-run frame
measurements, not isolated GPU material timings; run-to-run variation applies.
Fully covered training woodland skips the previous meadow material sampling,
and substantially fewer grass instances are submitted in forest interiors.

## Generation prompt

Use case: photorealistic-natural. Asset type: square game terrain ALBEDO material atlas, exact 2 by 2 equal quadrants with no borders or gaps. True orthographic top-down detailed scanned appearance of Estonian / northwestern Russian woodland floor, each quadrant representing roughly 1.5 metres square at identical scale. TOP LEFT: dense low feather moss carpet, muted olive and forest green, tiny intricate branching fronds, small dark earth gaps, no grass. TOP RIGHT: accumulated brown Norway spruce and Scots pine needle litter, slender short individual needles, tiny twigs, dark organic soil, no green vegetation. BOTTOM LEFT: weathered small curled birch and aspen leaf litter, naturally varied grey brown/tan leaves, some fragmented and damp, dark earth visible, no bright autumn colors. BOTTOM RIGHT: exposed dark sandy forest soil, fine grains with sparse minute root fibres and pebbles, no plants. Uniform diffuse overcast illumination, flat neutral exposure, no directional cast shadows, no highlights, no vignette, no perspective, no trees, flowers, large stones, text or labels. Each quadrant must fill exactly one quarter of the image, material extends to its boundaries, visually even distribution with subtle natural variation and no central focal motif. The top-left is moss, top-right needles, bottom-left leaves, bottom-right soil. Production quality photorealistic ground texture, not painted or stylized.
