# Imported elm collection

- Source: `_RESOURCES/elmtree/high_detail_elm_tree_collection.glb`, retained unchanged.
- Seven variants: 002, 005, 006, 007, 008, 009, 010; each gets one 10 m yard sample alongside the spruces.
- Offline forest: approximately half spruce, half elm; deterministic 20 m stands. No old procedural species returned.
- Shared 4096×2048 bark/foliage atlas; per-variant wood LODs and eight-direction far impostors.
- Near and middle LODs retain identical full leaf geometry; exporter verifies foliage hashes.
- Normalized trunk origins; existing approximate collision proxy retained. No online promotion.
- Rebuild with installed Blender: `--background --factory-startup --python tools/export_elm.py`.
- Attribution: Jagobo, CC BY 4.0; source/changes recorded in `models/textures/elm/ATTRIBUTION.md`.
- Release client/server build, 20/20 tests, 14 mesh-buffer validations and yard capture passed.
- World revision `0x20261036`.
- Benchmark launches ended before the timing sample completed; no performance delta claimed. Optimization remains deferred.
