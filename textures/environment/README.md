# Baltic environment material atlas

`baltic_material_atlas_1024.png` is a 1024x1024, 4x4 albedo atlas. Each tile is
256x256 pixels. Coordinates below are `(column, row)` from the top-left.

| Tile | Material |
| --- | --- |
| (0, 0) | Weathered grey concrete |
| (1, 0) | Cracked pale plaster |
| (2, 0) | Faded red brick |
| (3, 0) | Dark asphalt |
| (0, 1) | Cracked asphalt |
| (1, 1) | Packed dirt |
| (2, 1) | Damp mud |
| (3, 1) | Dry grass |
| (0, 2) | Mossy grass |
| (1, 2) | Beach sand |
| (2, 2) | Grey coastal rock |
| (3, 2) | Rusted painted steel |
| (0, 3) | Galvanized metal |
| (1, 3) | Aged dark wood |
| (2, 3) | Worn roofing felt |
| (3, 3) | Dirty off-white wall |

## Performance intent

- One texture bind can cover terrain, buildings, and common props.
- Generate mipmaps and use anisotropic filtering at runtime.
- Keep the source PNG for iteration; ship only the 1024 atlas initially.
- Add 2-4 pixels of duplicated padding around atlas tiles before enabling mipmapped
  production use, otherwise distant surfaces can bleed across tile boundaries.
- Later release builds should convert this to a GPU-compressed platform format.

This first generated atlas is an art-direction prototype. Individual tiles should
be seam-tested in engine before being treated as final production materials.

## Vegetation billboard atlas

`baltic_vegetation_atlas_1024.png` is a transparent 1024x1024, 4x4 atlas. Each
tile is 256x256 pixels. Coordinates are `(column, row)` from the top-left.

| Tile | Plant |
| --- | --- |
| (0, 0) | Dry grass A |
| (1, 0) | Dry grass B |
| (2, 0) | Meadow grass A |
| (3, 0) | Meadow grass B |
| (0, 1) | Reed grass |
| (1, 1) | Cattails |
| (2, 1) | Low scrub A |
| (3, 1) | Low scrub B |
| (0, 2) | Broadleaf shrub A |
| (1, 2) | Broadleaf shrub B |
| (2, 2) | Birch sapling A |
| (3, 2) | Birch sapling B |
| (0, 3) | Small coastal pine |
| (1, 3) | Dead shrub |
| (2, 3) | Fern clump |
| (3, 3) | Sparse coastal weed |

Render these on crossed camera-facing cards with alpha testing, not conventional
alpha blending. Instance cards by atlas tile and distance band. Suggested LODs:

- 0-20 m: two crossed cards for grass/shrubs, three for saplings.
- 20-60 m: one camera-facing card.
- Beyond 60 m: cull small plants; retain only tree silhouettes where useful.

Generate alpha-aware mipmaps to keep thin leaves from disappearing. The keyed and
full-resolution source images are development files; ship only the RGBA atlas.

## Building facade atlas

`baltic_facade_atlas_1024.png` is a 1024x1024, 4x4 facade atlas. Each 256x256
tile represents one modular wall bay. Coordinates are `(column, row)` from the
top-left.

| Tile | Facade module |
| --- | --- |
| (0, 0) | Concrete apartment window |
| (1, 0) | Boarded concrete window |
| (2, 0) | Plaster double window |
| (3, 0) | Brick industrial window |
| (0, 1) | Concrete apartment door |
| (1, 1) | Plaster service door |
| (2, 1) | Brick roller door |
| (3, 1) | Concrete loading doors |
| (0, 2) | Concrete balcony |
| (1, 2) | Plaster balcony |
| (2, 2) | Concrete stairwell window |
| (3, 2) | Brick utility vent |
| (0, 3) | Blank concrete infill |
| (1, 3) | Blank plaster infill |
| (2, 3) | Blank brick infill |
| (3, 3) | Corrugated metal infill |

Use facade tiles on shared unit wall quads or simple box buildings. Batch buildings
by atlas and render the window/door detail as texture at medium and far range. Only
nearby enterable buildings should replace these baked modules with actual geometry.
Add duplicated tile padding before mipmapped production use; the current source has
thin dark dividers at tile boundaries and is intended as a first visual prototype.

## Environmental decal atlas

`baltic_decal_atlas_1024.png` is a transparent 1024x1024, 4x4 atlas. Each tile
is 256x256 pixels. Coordinates are `(column, row)` from the top-left.

| Tile | Decal |
| --- | --- |
| (0, 0) | Rain streaks |
| (1, 0) | Damp wall stain |
| (2, 0) | Soot plume |
| (3, 0) | Mineral runoff |
| (0, 1) | Moss patch |
| (1, 1) | Algae smear |
| (2, 1) | Plaster exposing concrete |
| (3, 1) | Plaster exposing brick |
| (0, 2) | Concrete cracks |
| (1, 2) | Asphalt cracks |
| (2, 2) | Rust drips |
| (3, 2) | Peeling paint |
| (0, 3) | Muddy tire tracks |
| (1, 3) | Muddy boot prints |
| (2, 3) | Faded road marking |
| (3, 3) | Oil stains |

Batch decals by atlas and use polygon-offset overlay quads. Prefer alpha testing for
hard chipped surfaces and conventional alpha blending for stains/runoff. Cap visible
decals per spatial cell and cull small decals aggressively by projected screen size.
Several soft edges retain minor chroma contamination; clean these before treating
the atlas as a final shipping asset.

## Close grass atlas

`close_grass_atlas_1024.png` is a transparent 1024x1024, 4x4 atlas dedicated to
near/mid grass rendering. Each tile is 256x256 and shares a consistent root baseline.

- Row 0: short olive meadow patches.
- Row 1: tall mixed meadow and dry grass.
- Row 2: dry straw and seed-head patches.
- Row 3: broadleaf weeds, flattened grass, and sparse transition grass.

The atlas is intended for a separate grass renderer, not the mixed tree/bush pass.
Near LODs should use reusable blade/strip patch meshes; mid LODs should use card
clusters sampling these tiles. The generated source retains some pink edge spill and
must receive a final edge-color cleanup before shipping.
