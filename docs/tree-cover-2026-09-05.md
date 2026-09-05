# Tree cover — 2026-09-05

Bullets now stop at spruce trunks in the shared offline/server simulation. The collider uses the rendered six-sided taper, instance scale, elevation and rotation. Both mesh LODs share the trunk dimensions with collision; small end caps close the previously open mesh. Branch cards and foliage remain shootable. The wider movement cylinder is unchanged.

The existing placement grid supplies candidates for the entire swept segment, padded by the largest trunk radius computed at map startup. Convex clipping finds the nearest entry, which competes with terrain, boxes and posed/rewound player regions. Queries allocate no memory. Projectile code was moved out of `physics.cpp` into `projectiles.cpp`; both are now under 300 lines.

## Verification

- Before connecting the new sweep, the protected-target regression failed: the player behind the trunk lost health and no world impact was produced. It passes after integration.
- Geometry checks cover fast crossings, face grazing at three rotations, taper, top/bottom ends, starts inside, zero-length segments, foliage gaps, nearest trees and grid boundaries.
- Shared simulation checks cover a protected target with and without server rewind, an exposed target before the trunk, a closer box and an unobstructed shot through foliage.
- Client and headless server built in default and Release configurations. All four CTest suites passed in each, including the localhost UDP firing integration suite. Tree-specific server proof exercises the shared simulation with a rewind callback; the UDP suite covers existing firing behavior.
- Release game launched and a forest reference capture was inspected at 2560×1440, medium quality. The existing texture-unit warning remains.

## Collision microbenchmark

Run `./build-release/tree_collision_tests --bench` after a Release build. On this local macOS machine: 41,094 Paldiski trees, 168 trees in the densest grid bucket. The benchmark repeats 256 seven-metre segments near that bucket 1,000 times, mixing direct shots and gaps. Grid results matched a brute-force traversal for all 256 segments.

Measured mean: **0.882 µs per query**, or **0.226 ms for 256 queries**. This is an isolated collision measurement, not full server tick time, GPU timing, or a 16-client load test.

Online impact decals still use the existing axis-quantized normal packet, so their orientation on slanted trunk faces is approximate. Surface-specific wood effects remain a separate roadmap item. Packet layout and deterministic scatter are unchanged.
