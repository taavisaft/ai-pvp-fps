# Development records

Use dated reports for measurements and inspections; [TODO.md](TODO.md) owns current priorities. Old reports describe the state when measured and should not silently be rewritten as current claims.

- [2026-10-09: five-storey Khrushchevka exterior](docs/khrushchevka-exterior-2026-10-09.md) — eight sections, real apertures and shared collision, batched distance meshes, training placement and regression checks.

- [2026-09-25: native-resolution traversal baseline](docs/traversal-performance-2026-09-25.md) — repeatable pond/forest/scope routes, per-frame CSV, opt-in GPU diagnostics, and grass cost isolation. Grass rendering is the next measured target.

- [2026-09-22: spruce crown density](docs/spruce-density-2026-09-22.md) — overlapping inner foliage retained across LODs, distant needle-area regression checks.

- [2026-09-22: connected spruce model](docs/spruce-model-2026-09-22.md) — continuous leader, connected bough/twig hierarchy in both LODs, geometry attachment checks.

- [2026-09-22: tree proportions and palette](docs/tree-tuning-2026-09-22.md) — spruce needle alignment, lighter short-shoot foliage and subdued loading-area lighting.

- [2026-09-21: connected broadleaf branches](docs/tree-attachments-2026-09-21.md) — attached twig/leaf structure, shared wind and geometry regression checks.

- [2026-09-21: pond reflection continuity](docs/pond-reflection-2026-09-21.md) — cached-camera projection, invalidation and regression checks.

- [2026-09-14: Paldiski grass scale test](docs/paldiski-grass-2026-09-14.md) — streaming meadow pool and main-map material comparison.
- [2026-09-13: full lobby grass](docs/lobby-grass-coverage-2026-09-13.md) — expanded coverage and controlled Release comparison.
- [2026-09-05: game inspection and HUD performance improvement](docs/progress-2026-09-05.md) — controlled local benchmark and connection smoke test.
- [2026-09-05: documentation review](docs/documentation-review-2026-09-05.md) — stale claims, verified facts, and refreshed roadmap.
- [2026-09-05: server-enforced firing rules](docs/server-firing-2026-09-05.md) — protocol v4, bounded firing requests, state epochs and headless/UDP regression coverage.
- [2026-09-05: soldier and Uzi visual pass](docs/soldier-uzi-2026-09-05.md) — editable Blender assets, shared first-/third-person Uzi, distance meshes, visual checks and Release comparison.
- [2026-09-05: tree cover](docs/tree-cover-2026-09-05.md) — shared tapered-trunk sweeps, regression coverage and dense-stand query cost.
- [2026-09-05: forestry combat site, first stage](docs/forest-site-2026-09-05.md) — enterable shelter, staggered timber cover, movement checks and frame comparison.
- [2026-09-05: lobby meadow trial](docs/lobby-meadow-2026-09-05.md) — finer terrain, layered ground materials and bounded grass, awaiting visual review.
- [2026-09-05: dense meadow reference patch](docs/dense-meadow-2026-09-05.md) — continuous grass coverage, plant variants, cast shadows and delayed GPU measurements.
- [2026-09-06: Retina/standard-DPI switching](docs/display-scaling-2026-09-06.md) — refresh backing dimensions after events; tests pass, but the user still reproduces the display-transition problem.

- [Training meadow reference pass and generated ground texture](docs/training-meadow-asset-2026-09-14.md) — 2026-09-14.

- [Training spruce realism and fuller crown trial](docs/training-trees-2026-09-14.md) — 2026-09-14.

- [Four spruce types mixed through training](docs/training-spruce-types-2026-09-15.md) — 2026-09-15.

- [Mixed training woodland: ash, birch, oak and spruce](docs/training-mixed-woodland-2026-09-15.md) — 2026-09-15.

- [Training landscape: meadow, pond, wooded hills](docs/training-landscape-2026-09-17.md) — 2026-09-17.

- [Vegetation, ground and haze ported to Paldiski](docs/training-landscape-2026-09-17.md#paldiski-port-same-day-world-revision-0x20260918) — 2026-09-17. World revision 0x20260918.

- 2026-10-17: player animation clips — idle (breathing, weight shift), walk, run (lean, knee lift, longer cycle), crouch-walk, jump/land, blended by measured speed in `src/player_anim.h`; server hit pose unchanged; `FPS_MOVE` scripted-input capture aid; `player_anim` test.

- 2026-10-17: third-person weapon hold made right-handed — trigger hand is now the right arm, gun sits off the right shoulder (`GUN_HOLD_HIP/ADS` in game.h), toed in 9°/3°, head rolls onto the stock when aiming, fists 10 cm. Hit regions for arms/hands/gun move with it (shared pose); server and client rebuild together.

- 2026-10-17: one body for both views — the separate first-person view model is gone; first person draws the shared posed body from the right eye (`EYE_RIGHT`), aiming anchors the gun to the eye line in every view (`WeaponVisual::adsHold`, test `player_anim` checks the sight sits on the view line), recoil and muzzle flash ride the pose. Capture runs (`FPS_SHOT`/`FPS_BENCH`) now ignore live input; earlier odd captures were keypresses from the desk.
