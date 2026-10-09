# Grass and gravel footsteps

These are free CC0 audio assets, not AI-generated audio. Acquired 2026-10-09.
Original compressed sources are retained in `source/`.

| Bank | Author / source | Included files |
| --- | --- | --- |
| Grass | Kenney, [Impact Sounds 1.0](https://kenney.nl/assets/impact-sounds) | Five `footstep_grass_000.ogg`–`004.ogg` recordings converted to `grass_01.wav`–`05.wav` |
| Gravel | TinyWorlds, [Different steps on wood, stone, leaves, gravel and mud](https://opengameart.org/content/different-steps-on-wood-stone-leaves-gravel-and-mud) | One `gravel.ogg` recording processed into eight `gravel_01.wav`–`08.wav` variations |

Both source pages explicitly mark their assets CC0. Kenney's original license
notice is retained in `Kenney-LICENSE.txt`. TinyWorlds states that the sounds are
edited recordings from pdsounds.org. License reference:
https://creativecommons.org/publicdomain/zero/1.0/

Downloaded archives (SHA-256):

- `kenney_impact-sounds.zip`: `029d734af1582474edf3a694d1b0cebc97c1c152f2f39fa34d4c2bafc5de77f8`
- `[kdd]DifferentSteps_0.zip`: `987d072b7deaa65177e005ddd8185f2351be7c9a76cd7575dea560549c39b33b`

## Rebuilding and playback

Run `python3 tools/prepare_footsteps.py` with ffmpeg on PATH. This trims leading
silence and long releases, filters extreme frequencies, applies short fades and
balances levels with a peak ceiling of 0.5. Gravel variants use slightly different
resampling rates and low-pass cutoffs; they are derived from one original contact,
not eight independent recorded takes. Outputs are mono 44.1 kHz, 16-bit PCM WAV.

The client preloads up to eight files per bank at startup. It uses only available
recordings when any load successfully; an entirely missing bank gets procedural
fallbacks. Playback avoids immediately repeating a variant when more than one is
available, with small gain and pitch variation. It allocates no sample buffers
during play. Asset-only builds refresh the binary's sound directory.

Grass is the terrain default. The lobby's worn range and trail use gravel as an
initial proxy for their dirt surface, using the same `lobbyWear` mask as rendering.
No actual gravel terrain is added. Existing solid box tops keep the neutral
contact fallback; wood/metal/concrete banks, water and remote-player footsteps
remain future work. Cadence follows collision-resolved local simulation movement,
so blocked movement and network corrections do not produce steps.

In-game sound quality still needs listening feedback: walk/sprint between the
range at (12,0), the trail at (0,19), and grass at (0,30).
