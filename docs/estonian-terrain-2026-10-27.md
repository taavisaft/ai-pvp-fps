# Estonian terrain — 2026-10-27

Reference: seven countryside photographs supplied in chat.

- Offline landscape: broad low relief replaces the enclosing northern/side ridges.
- Pond, practice hills and building apron retained; shared rendering/collision heightfield.
- World revision: `0x20261028`. Online terrain unchanged.
- Release client/server built; 20/20 tests passed. Meadow capture visually checked.
- Field regression: under 7 m total relief and 0.15 m rise per metre outside 220 m.
- Woodland: broad irregular stands, open fields and map-wide tree/shrub islands.
- Trees: 51,425 → 34,011; birch 1 → 10,799, mixed with spruce, ash and oak. Pine asset pending.
- Medium meadow, 3456×2234, same camera: 12.81 → 12.82 ms (+0.01 ms, +0.08%); single runs, no demonstrated speedup.
- Meadow/northern captures checked. Weather and ground colours remain subsequent passes.
