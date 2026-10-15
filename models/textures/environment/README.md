# Environment assets

Inventory reviewed on 2026-09-05.

| File present | Current use |
| --- | --- |
| `woodland_floor_atlas.png` | Lobby moss, needles, leaf litter and soil. See [provenance and prompt](woodland_floor_atlas.md). |
| `forest_ground_1k.jpg` | Original Paldiski forest-floor photo, still used online. |
| `baltic_material_atlas_1024.png` | Art-direction prototype. No reference to this atlas exists in the current renderer/material loading code. |

The old document also described vegetation, facade, decal and close-grass atlases. Those files are not in this directory and their proposed rendering paths should not be assumed implemented. The original tile plans are preserved in [the historical atlas plan](../../docs/archive/environment-atlas-plan.md).

The live vegetation renderer loads bough and broadleaf textures, and bakes
per-species tree impostors at startup. Streamed meadow geometry is active;
woodland interiors suppress it through the shared visual habitat mask.

Before adopting the prototype material atlas, verify seamless edges, mip padding, exact asset provenance, and visual quality in a small test area. Establish measured CPU/GPU budgets before adding more atlas passes or density.
