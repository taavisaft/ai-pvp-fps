# Image textures (optional)

Drop **seamless / tileable** images here to override the procedural material textures.
Loaded automatically at startup; if a file is absent the procedural generator is used,
so these material images are optional. This does not apply to vegetation assets:
`spruce_branch.png` and `bush_1.png` are separately loaded cutout textures, and
vegetation initialization fails if either is missing.

Expected filenames (`.png` or `.jpg`, square recommended):

| File           | Material  | Notes                                            |
| -------------- | --------- | ------------------------------------------------ |
| `ground.*`     | ground    | also replaces the procedural grass shader        |
| `concrete.*`   | concrete  |                                                  |
| `metal.*`      | metal     |                                                  |
| `wood.*`       | wood      |                                                  |
| `rock.*`       | rock      |                                                  |

Only the **Color / Albedo** map is used (no normal/roughness/AO). Good CC0 source:
https://ambientcg.com — grab a set, use its `*_Color.jpg`, rename to the table above.

`ground.jpg` — existing attribution: seamless grass (Pexels / SeamlessTextures).
The exact source URL, author and license are not recorded here yet; verify those
before redistributing this asset. Do not assume this file has the CC0 license of
the example source above.

The material loader prefers `.png` over `.jpg` when both exist. Ground tiles are
currently 3.5 m, with neutral image tint; the photo replaces the procedural grass
base on Paldiski too. The separate forest-floor image is documented in
[environment/README.md](environment/README.md).

Textures and sounds are copied beside the executable after client relinking.
An asset-only edit may require manually refreshing the build copy; shaders have
an always-run copy target.
