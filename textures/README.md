# Image textures (optional)

Drop **seamless / tileable** images here to override the procedural material textures.
Loaded automatically at startup; if a file is absent the procedural generator is used,
so the game runs fine with this dir empty.

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

`ground.jpg` here is ambientCG **Grass001** (CC0).
