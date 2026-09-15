# Four training spruce types — 2026-09-15

Extended by the [mixed woodland pass](training-mixed-woodland-2026-09-15.md).

Four separately generated crown meshes share the existing needle texture and
unchanged collision trunk. Training-only assignment hashes each tree's fixed
world position, so the mix is stable on restart and identical for both mesh
LODs, shadows, and far impostors. Paldiski and grass are unchanged.

| Type | Shape | Triangles | Training count |
| --- | --- | ---: | ---: |
| Full | Dense balanced taper | 5576 | 11 |
| Narrow | Slim crown, closer tiers | 4440 | 7 |
| Broad-drooping | Wide, heavily hanging boughs | 6480 | 5 |
| High-crown | Sparse elevated foliage and exposed lower limbs | 2432 | 12 |

Profiles and stable assignment: src/training_spruce.h. Geometry builder:
src/training_spruce_mesh.cpp. Startup uploads, per-type batching and cleanup:
src/training_tree.cpp. Each type has its own startup-baked far silhouette within
a 0.76×1.10 envelope. Per-instance width and crown-base variation remain;
impostors represent the type rather than every individual variation.

Drawing filters existing visible-instance streams into a fixed 128-instance
staging array, flushing bounded chunks. No new heap allocation during drawing.
Maximum four populated batches per tree pass. Both mesh LODs use the same
per-type geometry to preserve their complementary fade; future large-map
rollout still needs a forest-scale performance check.

Validation: both build/game and build-release/game rebuilt; training_spruce,
tree_collision and lobby_ground tests passed. New headless mesh checks cover
finite vertices, valid indices, conservative bounds, distinct crown dimensions,
unchanged shared trunk geometry, repeatable generation and four-way assignment
on negative/positive coordinates. A failing bounds check caught the first broad
crown's excessively low branches; its base was raised before the final run.

Runtime confirmed all four types in the map, and the mixed crowns were visually
checked. Stationary Release medium, 2560×1440, camera (10,30), yaw90/pitch5,
300 warm-up + 600 samples: mean8.27ms, p50 8.29ms, p95 9.63ms, max13.80ms.
Single-view measurement, not GPU pass time or online-map performance proof.
Preview: build/four-spruces-preview.png. Log: build/four-spruces-benchmark.log.
