# Connected spruce model — 2026-09-22

Replaced independently positioned spruce sprays with a connected procedural model shared by both maps and all four spruce profiles. A continuous upper leader overlaps the unchanged collision trunk. Each bough consists of two tapered wood segments, with lateral twigs rooted on their exact piecewise centerline. Needle cards start at wood tips, aligned to the texture's opaque stem (U=.48, V=.01). Inner foliage grows from branch elbows; the top shoot grows from the leader. Both full and low meshes retain connected wood. The shared spatial wind function remains in the lit and depth passes.

The earlier model placed lateral sprays away from the bough with no connecting wood, and had no leader supporting the upper tiers. The preceding palette/tip tuning did not fix those structural defects; this rebuild supersedes that spruce construction.

## Verification

Release build and all 10 CTests passed. Added geometric connectivity checks for all four spruce profiles in both LODs. Negative cases deliberately detach a spray and a bough; both are rejected. Existing tests verify trunk preservation, mesh bounds, finite values and deterministic output. Re-ran spruce and woodland tests after strengthening the negative cases and bringing the terminal shoot down into the upper whorl to remove a visible crown-tip gap.

Full/low triangles: full spruce 7262/3130; narrow 6590/2842; drooping 7514/3238; high crown 3398/1762. Full meshes stay below the existing 8000-triangle budget. No new texture, draw pass or per-frame allocation. Low meshes cost more geometry because their wood is now connected too.

Matched medium 2560×1440 meadow benchmark: previous model 9.37 ms average / 12.96 ms p95; connected model 9.42 / 11.78 ms. Measured before the final crown-tip overlap adjustment (same triangle count). Single local CPU submission/wait timing pair; does not establish dense-forest GPU performance. Close and whole-tree views captured in `build/spruce-connected-model.png` and `build/spruce-connected-whole.png`.

Needles still use alpha-cutout cards, so edge-on views can reveal flat surfaces. The solid branch hierarchy is now modeled rather than implied by independently floating photographs.
