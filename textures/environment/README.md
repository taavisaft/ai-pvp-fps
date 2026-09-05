# Environment assets

Inventory reviewed on 2026-09-05.

| File present | Current use |
| --- | --- |
| `forest_ground_1k.jpg` | Loaded by `MaterialLib::init` as the forest-floor texture; falls back to ground when absent. |
| `baltic_material_atlas_1024.png` | Art-direction prototype. No reference to this atlas exists in the current renderer/material loading code. |

The old document also described vegetation, facade, decal and close-grass atlases. Those files are not in this directory and their proposed rendering paths should not be assumed implemented. The original tile plans are preserved in [the historical atlas plan](../../docs/archive/environment-atlas-plan.md).

The live vegetation renderer instead loads `../spruce_branch.png` and `../bush_1.png`, and bakes a tree impostor at startup. Grass geometry exists but is disabled in all current quality tiers.

Before adopting the prototype material atlas, verify seamless edges, mip padding, exact asset provenance, and visual quality in a small test area. Establish measured CPU/GPU budgets before adding more atlas passes or density.
