# Paldiski grass scale test — 2026-09-14

The current lobby meadow blades and grass-ground pattern now run on Paldiski,
including online play. Launch offline with `FPS_MAP=paldiski ./build/game`.
`FPS_NOMEADOW=1` disables meadow geometry for comparison. The old sparse-grass
quality flag no longer controls Paldiski; all qualities use the same meadow.
Grass casts no shadows, receives world shadows, and remains cosmetic.

A fixed 24×24 pool of five-metre tiles reuses GPU buffers, VAOs and rank storage.
Up to six nearest-first tiles are refilled per frame. Seeds depend on world tile
coordinates, so revisiting a tile recreates its placement. The refill uses fixed
stack arrays and existing buffer storage; no container growth or GL object
creation while walking. Resources are allocated on map initialization. Density
thins per-clump from 40 to 50 m, inside a roughly 55 m preload region. The terrain
supplies coverage farther away. Spawn/teleport can require several frames to fill
nearby tiles; steady moving/teleport stress tests remain outstanding.

Terrain retains sand, dirt, rock, forest-floor and snow blending. Only its grass
base uses the same procedural strand pattern as the lobby. Placement excludes
low shoreline elevations and box footprints, and reduces grass with rock slope,
dirt and forest masks. No deterministic gameplay-world or protocol changes.

## Verification

Both normal and Release builds completed. Headless lobby regression covers the
streamed density boundary and unique pool slots across negative/positive tile
coordinates. Bog grass and a grass-free shoreline were visually inspected.

Release medium, 2560×1440, fixed `FPS_REF=bog`, overcast, same hardware. 300 warm-up
frames then 600 samples. Both runs use the new terrain material.

| Geometry | Mean ms | Median ms | p95 ms | Max ms | FPS from mean |
| --- | ---: | ---: | ---: | ---: | ---: |
| Meadow enabled | 10.91 | 10.93 | 13.55 | 20.84 | 92 |
| Meadow disabled | 6.86 | 6.53 | 12.70 | 59.36 | 146 |

The added mean cost in this view is 4.05 ms. CPU submission/wait world means were
.61 ms enabled and .27 ms disabled; these are not GPU timings. This is a stationary
single-view comparison, not proof of moving-stream performance or online load.
