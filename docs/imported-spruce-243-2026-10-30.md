# Norway spruce 243

- User source: `_RESOURCES/spruce/realistic_hd_norway_spruce_243.glb`, unchanged. License not inferred.
- Rebuild with installed Blender: `--background --factory-startup --python tools/export_spruce.py -- 243`.
- Output: `models/textures/spruce243/`, 2048² albedo/alpha atlas; 34,618 / 7,688 triangle LODs and far impostor.
- Yard: `(32,125)`, beside model 1843 at `(18,125)`; both samples 10 m tall.
- Offline terrain: deterministic mix, 20,120 of model 243 and 20,527 of model 1843. Old species remain absent offline.
- Existing approximate trunk collision retained; no online promotion. World revision `0x20261035`.
- Release build, 20/20 tests, buffer validation and yard capture passed.
- Same medium meadow benchmark: 23.60 → 22.52 ms (−1.08 ms, −4.6%); single runs, not an isolated GPU comparison.
