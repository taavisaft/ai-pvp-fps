# Supplied Norway spruce

- Source: `_RESOURCES/spruce/realistic_hd_norway_spruce_1843.glb`, retained unchanged. User-supplied asset; license not inferred.
- Rebuild: installed Blender, background mode, `--python tools/export_spruce.py`.
- Output: `models/textures/spruce/`: 2048² base-colour/alpha atlas, 65,371 / 14,522 triangle mesh buffers, source dimensions.
- Offline test: all 40,650 tree instances now use this spruce. Online assignment retained.
- Existing collision proxy retained; imported trunk is not an exact collision match. No online promotion.
- Exclusive LOD selection removes checkerboard transitions; zoom extends mesh ranges. Switches can pop.
- Release build, 20/20 tests, finite/indexed buffer checks and woodland capture passed.
- Medium meadow comparison: 15.25 → 15.70 ms (+0.45 ms, 3.0%); includes LOD changes, single runs. Scope cost unmeasured.
- Preview: `FPS_REF=woodland FPS_QUALITY=medium ./build/game`.
- Yard sample: `(18,125)`, 10 m tall, near the entrance. World revision `0x20261032`; placement/collision tests pass.
- Yard catalogue now contains one sample per imported model, currently this spruce only. Old catalogue trees removed from placement; old assets retained. World revision `0x20261033`.

## All-spruce performance test

Release, medium, 3456×2234, same reference cameras and atmospheres. Default frame timing; single runs, no isolated GPU attribution.

| View | Mixed trees | All imported spruce |
|---|---:|---:|
| Meadow/pond | 15.29 ms | 23.02 ms (+7.73 ms, 50.6%) |
| Forest edge | 12.86 ms | 23.03 ms (+10.17 ms, 79.1%) |

All 20 tests pass. Further LOD/foliage optimization needed at this density; test placement is retained for inspection.
