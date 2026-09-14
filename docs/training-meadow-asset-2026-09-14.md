# Training meadow reference pass — 2026-09-14

Target: user-selected fourth reference (Arma Contact forest meadow). Training
only. Original Paldiski mesh index ranges and material binding retained.

New tapered geometry mixes 34 green/dry blades, occasional branched seed heads
(12% of tufts), and broad leaves (6%). Near 384 triangles, far 40 triangles per
tuft, with static distant blades and existing distance-density control. Detailed
and far training meshes are appended to startup buffers; online draws use only
the original index ranges. No additional draw calls or per-frame uploads.

New ground asset: textures/training_meadow_ground.png. Generated with the built-in
imagegen tool, original output copied unmodified (1254×1254 RGB despite requested
1024²). It supplies flattened straw/grass detail; the shared growth field controls
the green/dry tint and standing grass population. It is an albedo-only first pass;
no measured normal/roughness maps. Artist review still needed for repetition and
the ground/plant colour transition.

The new width exposed an RGB row-alignment bug in uploadTextureRGB: OpenGL's
default four-byte unpack alignment read beyond stb's tightly packed rows. Crash
report pointed to glTexImage2D via uploadTextureRGB. Upload now sets alignment1
and restores previous state. Subsequent game preview loaded successfully.

## Exact generation prompt

Use case: photorealistic-natural. Asset type: seamless square game terrain albedo texture, 1024x1024. Create a photorealistic orthographic straight-down scan of one square metre of temperate European meadow floor. Dense finely tangled flattened dead grass and thin straw interwoven with short muted olive-green grass, some tiny dry brown leaf fragments and small irregular exposed grey-brown soil gaps. Roughly 45 percent faded beige straw, 40 percent subdued olive green low grass, 15 percent soil. Fine intricate fibrous detail, realistic scale, natural chaotic direction. Even overcast diffuse illumination, no directional shadows, no ambient vignette, no highlights, no perspective, no horizon, no upright plants, no large leaves, no rocks, no text. All four edges must tile seamlessly. Restrained earthy colours, not saturated lime green, not smooth lawn. This is a NEW original material asset for a realistic forest meadow game.

## Verification

Both build/game and build-release/game rebuilt successfully. Focused lobby_ground
regression passed; git diff --check passed. Runtime shader compilation and texture
loading verified through screenshots. Release medium, 2560×1440, fixed training
camera (10,30), yaw90, pitch-10: 900-frame run (300 warm-up + 600 samples) averaged
9.61 ms, p50 10.02 ms, p95 13.67 ms, max16.09 ms. This is a single stationary
view, not a before/after comparison or a GPU-pass timing claim. Preview:
/tmp/fps-contact-meadow.png; log: /tmp/fps-contact-meadow.log.
