# Dark sunset

- K cycles to Dark Sunset; launch directly with `FPS_ATMO=sunset ./build/game`.
- Sun elevation about 2°, enlarged 1.6° disk, brighter gold centre and warm horizon glow.
- Cool low ambient fill, dark foreground and existing long directional shadows. No extra render pass or texture.
- Existing far-shadow cap retained. Default starting weather unchanged.
- Release build, 20/20 regression tests, yard sunset capture checked.
- Medium meadow, 3456×2234: Golden Hour 26.12 ms; Dark Sunset 27.04 ms (+0.92 ms, 3.5%). Single runs, no isolated GPU attribution.
