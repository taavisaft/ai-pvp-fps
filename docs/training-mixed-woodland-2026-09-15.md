# Mixed training woodland — 2026-09-15

The training map now mixes the four existing spruce profiles with ash, birch,
and oak broadleaf crowns. Existing positions, scales and collision trunks are
preserved. Species are chosen deterministically from world position and remain
consistent across lit geometry, shadows, and distant impostors. Grass and
Paldiski rendering remain unchanged by this pass.

The training ring contains 14 spruces (4 full, 1 narrow, 2 drooping, 7 high-crown),
9 ash, 5 birch and 7 oak trees. Broadleaf crowns use forked woody scaffolds and
many small bent foliage sprays, with species-specific spread and leaf atlas
coordinates. Birch has a pale bark palette with dark horizontal marks. New
materials are an artistic approximation, not scans or copied game assets.

New asset: textures/training_broadleaf_atlas.png, 2019×779 RGBA. Three columns:
ash, birch, oak. Generated with the built-in imagegen tool and copied unchanged;
alpha preserved. Existing texture loader handles RGB bleed and mip generation.
Each tree type has a separately baked distant silhouette with its own width.
Seven fixed type batches reuse the existing 128-instance staging buffer; no
new heap allocation during tree drawing. Mesh generation runs at startup.

Both build/game and build-release/game rebuilt. training_woodland,
training_spruce, tree_collision and lobby_ground tests passed. New headless
checks cover finite geometry, index bounds, atlas-cell confinement, conservative
capture bounds, unchanged trunk positions/normals, repeatable generation and
species assignment. The final runtime preview was visually inspected.

Stationary Release medium, 2560×1440, camera (10,30), yaw90/pitch5, 300 warm-up
+ 600 samples: mean8.53ms, p50 8.41ms, p95 11.75ms, max16.70ms. This measures
one training view, not full-map performance or GPU pass time. Preview:
build/mixed-woodland-preview.png. Log: build/mixed-woodland-benchmark.log.
The supplied Arma/DayZ references remain the visual target; user review needed.

## Crown shape and interior lighting follow-up

User feedback: preserve the leaf artwork, reduce the artificial bouquet shape,
and darken the shaded crown interior. Broadleaf sprays are now smaller, with
independent orientation and roll, irregular lobe heights and rounded lobe normals.
The upper crown tapers more. Ash/oak use 3,996 triangles each and birch 2,714.
The original leaf atlas is unchanged.

Training foliage uses species-sized canopy occlusion: direct sunlight, sky fill,
and transmitted light diminish toward the core. Broadleaf albedo is reduced 10%,
spruce 4%; outer foliage retains sunlight. This is a cheap spatial approximation,
not ray-traced self-shadowing. The distant albedo bake retains approximate canopy
occlusion. No new shadow passes, textures, uniforms or draw-time allocations.
Grass, main-map rendering, placement and collision are unchanged by this follow-up.

Both builds and the same four focused tests passed. Runtime captures were
inspected in golden light close up and clear daylight at the fixed wider view.
Close comparison: build/tree-before-preview.png and build/tree-after-preview.png.
Wide view: build/tree-crown-wide-preview.png. Release medium at 2560×1440,
camera (10,30), yaw90/pitch5, 300 warm-up + 600 samples: mean9.11ms,
p50 9.37ms, p95 12.41ms, max16.03ms. Log: build/tree-crown-benchmark.log.
This single run is not a controlled performance-regression measurement against
the earlier 8.53ms run, nor a GPU pass measurement or large-map result.

Distant runtime smoke check at (10,-340), yaw90/pitch1 exercised 35 impostors
and 13 overlapping LOD1 instances. No shader errors; distant silhouettes render.
The wide field of view makes this a rendering smoke check, not detailed LOD
transition validation. Capture: build/tree-crown-distant-preview.png.

## Exact built-in imagegen prompt

Use case: photorealistic-natural. Asset type: original transparent foliage atlas for a realistic 3D forest game. Landscape image divided into exactly THREE equal-width vertical cells in a single row. Each cell contains ONE separate leafy branch spray, root at bottom centre, growing upward, fully inside its cell with generous clear transparent padding. Left cell: common European ash, slender woody twigs carrying pinnate compound leaves, many distinct narrow oval leaflets, muted olive green. Middle cell: silver birch spray, delicate reddish-brown fine twigs and many small triangular serrated green leaves, airy natural gaps. Right cell: English oak spray, brown branching twigs with broad rounded-lobed leaves, deeper natural green. Photographic botanical realism, clearly resolved individual leaves and their veins, natural irregular silhouettes, moderately dense sprays with real empty gaps. Even diffuse overcast illumination, albedo-like colours without hard shadows or glossy highlights. True transparent alpha everywhere outside the leaves and stems, including gaps. No backdrops, no ground, no pots, no labels, no text, no gridlines, no borders. Keep each spray strictly within its own third; no overlap between cells. All three sprays should occupy similar image height and scale, roughly 80 percent of cell height and width. This is a game asset atlas, not a scene or a picture of whole trees.
