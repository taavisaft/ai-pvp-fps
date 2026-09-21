# Connected broadleaf branches — 2026-09-21

User close-up showed detached textured branch sprays. The generator scattered
cards around cluster centres with unrelated orientation, without wood connecting
their stems. Six skirt clusters were unsupported, and the top cluster exceeded
the collision trunk. Different wood/card flex values also separated adjoining
pieces during wind.

Ash, birch and oak now emit supporting branches and tapered twigs. Each twig
starts on a parent segment, and its foliage card starts at its tip with the
existing atlas's stem position aligned to that point. Spray directions branch
out from their parent with controlled lift and roll. The skirt has supporting
limbs, the top has a leader overlapping the trunk, and both detail levels retain
connected structure. Shared spatial wind deformation replaces independent flex
for broadleaf wood and cards in both lit and shadow shaders.

No new leaf artwork, tree placement, collision or wire changes. Spruces unchanged.
Leaves still use textured cards; this fixes attachment and branch organization,
not the flatness of a card viewed edge-on. The open branch structure changes the
crown silhouette; it is not an identical distant outline to the old piled cards.

Full meshes: ash/oak 7,494 triangles, birch 5,504. Low meshes: 1,922 and 1,422.
An initial low mesh was too costly and was reduced before delivery. No additional
draw calls, texture lookups or per-frame allocations. Meshes and impostors are
still built at startup.

Verification: both Release targets built; all 10 CTests passed. After final
orientation/low-mesh tuning, training_woodland passed again. New emitted-mesh
checks require each wood root to intersect already-connected wood and each atlas
stem to meet its twig tip, for all three species and both LODs. Deliberately moving
a card off its twig fails that check. Existing trunk-envelope, finite-geometry,
index, atlas and capture-bound tests remain in place.

Runtime inspected close ash (58,166, yaw45 pitch25), close oak (48,148, yaw45
pitch28), and meadow. Final oak capture: `build/tree-oak-connected.png`.
Release medium, 2560×1440, 300 warm-up + 600 samples: final oak mean 7.73 ms,
median 7.54, p95 10.87, max 13.85. Meadow after LOD reduction, before the final
spray orientation adjustment: mean 9.54 ms, p95 12.70. Initial meadow mean 9.90 ms.
Single local stationary runs are not proof of a speedup or traversal performance;
these are frame intervals, not GPU pass timings.
