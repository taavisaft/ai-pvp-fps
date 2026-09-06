# macOS alpha packaging

Run `python3 packaging/macos/package.py` on an Apple Silicon Mac with Xcode
Command Line Tools, CMake and Python 3. The script downloads checksum-pinned
SDL 2.32.10, builds SDL and the game for arm64/macOS 12, runs CTest (requires
localhost UDP access), creates the icon sizes, bundles assets and SDL, checks
library paths, ad-hoc signs the app and writes a ZIP and SHA-256 file to `dist/`.
No App Store, Homebrew runtime or installer is needed by players.

The source PNG is `icon.png`; ICNS is generated during packaging. It was created
with the built-in image generation tool. Prompt: "Create a production macOS
game app icon for an independent realistic tactical FPS set in Estonian coastal
pine forests. A single bold worn olive tactical helmet silhouette over a sparse
dark pine forest and muted fog, restrained warm rim light, high contrast readable
at small sizes, premium painted realistic game emblem. Rounded square charcoal
olive background, generous safe margin. No text, letters, badges, flags, skulls,
or watermark. Icon artwork only, no mockup." Final edit replaced the generated
checkerboard corners with solid charcoal; the final asset is opaque.

The optional `textures/ground.jpg` is excluded because its repository attribution
requires verification before redistribution. Source assets stay untouched.
SDL and GLM licenses and existing texture attribution files are bundled.

This is an Apple Silicon alpha, not a verified universal build. macOS 12 is the
compiler deployment target, not a claim of testing on that OS. Local smoke tests
must use the extracted app from outside the repository, including a path with
spaces, and inspect a rendered frame. Verify the signature again after extraction.

For manual GitHub distribution, create a release and attach the ZIP and checksum
from `dist/`. Do not commit generated build or distribution directories. This
script does not publish, sign with Developer ID, or notarize. Ad-hoc signing
allows local execution but does not establish Gatekeeper trust for downloads.

## Local validation — 2026-09-06

Both Release targets built; all six CTest suites passed, including UDP integration.
The ZIP was extracted to `/tmp/FPS Package Test` and the app launched with `/tmp`
as the working directory. A 30-frame capture rendered the lobby, HUD, weapon,
terrain and vegetation, and exited successfully. The pre-existing texture-unit
warning remains. Both bundled Mach-O files report macOS 12.0 deployment targets;
SDL uses system libraries only. Ad-hoc signature verification passed after
extraction. This does not verify Gatekeeper behavior for an internet download,
macOS 12 hardware, Intel support, or multiplayer gameplay on another machine.
