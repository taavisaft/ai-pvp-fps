# Soldier and classic Uzi

Original Blender-authored game assets, created for this repository on 2026-09-05. No downloaded meshes, textures, fonts, add-ons, or external asset licenses are required. The Uzi exterior uses the [Small Arms Survey / Royal Armouries identification sheet](https://www.smallarmssurvey.org/sites/default/files/SAS_weapons-sub-machine-guns-Uzi.pdf) as a visual reference; none of its images are embedded.

Open **soldier_uzi.blend** in Blender. `Soldier - assembled preview` presents the model. `SOURCE - normalized parts and Uzi meters` contains the editable components; enable its visibility to edit. Assembled copies share source mesh data, so edit vertices in Edit Mode. `Uzi - product view` is a separate close-up collection. The studio previews use Cycles, not the game renderer.

The soldier has a helmet, goggles, balaclava, communication headset, plate carrier, webbing, magazine and utility pouches, gloves, field clothing, and boots. The Uzi has an exposed barrel/open muzzle, iron sights with an open aperture, charging handle, receiver details, ribbed grips, magazine, trigger guard, and folded stock. It is an exterior visual model, not a mechanical model.

Regenerate the original authored geometry and previews (overwrites the .blend and headers):

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --python tools/soldier/build.py
```

Export a saved Blender edit without rebuilding its geometry:

```sh
/Applications/Blender.app/Contents/MacOS/Blender tools/soldier/soldier_uzi.blend --background --python tools/soldier/export.py
cmake --build build
```

The compatibility entry point `tools/player_model.py` now invokes the current generator. The old `tools/player_model.blend` and `.png` are historical previews.

Each source object has an `asset_group` property. Export preserves its `Albedo` corner colors and the material's `specular` custom property. Negative specular marks camouflage fabric; the game uses its absolute value for highlights and generates a stable local-space pattern. Blender's baked color pattern is a studio approximation. Arbitrary Blender node graphs and texture images are not exported. Added geometry must have outward normals and positive-scale transforms. Paint `Albedo` to change exported colors; changing only the Principled shader does not repaint existing vertex colors.

Runtime format is ten floats per vertex: position, normal, linear RGB, specular. Parts occupy normalized pose space; the Uzi uses meters. The exporter converts Blender Z-up to game Y-up. Source order must match `Renderer::PlayerPartId`. Generated headers contain full and reduced geometry. All GPU buffers upload during renderer initialization. Each body segment is one draw; the entire Uzi is one draw, independent of the number of authored components.

| Geometry | Close triangles | Beyond 15 m |
| --- | ---: | ---: |
| Complete soldier | 26,556 | 6,564 |
| Uzi | 11,932 | 2,944 |
| Soldier + Uzi | 38,488 | 9,508 |

Both world and shadow passes choose detail from camera distance. First-person geometry always uses full detail. Hitboxes, weapon statistics, protocol, and shared gameplay pose remain unchanged; equipment is cosmetic and does not add armor protection. The model uses segmented posing, not a deforming skinned rig. Continuous shoulders/elbows, articulated fingers, a support hand in first person, reload/magazine animation, and higher-quality surface maps remain further art/animation work.

Useful offline checks (variables are opt-in; omit them for normal controls):

```sh
FPS_POS=22.3,6 FPS_YAW=156.5 FPS_PITCH=-13 ./build/game
FPS_ADS=1 FPS_SHOT_FRAME=300 FPS_SHOT=/tmp/uzi-ads.ppm ./build/game
FPS_CROUCH=1 FPS_LEAN=0.7 FPS_SHOT_FRAME=300 FPS_SHOT=/tmp/crouch.ppm ./build/game
FPS_WEAPON=glock FPS_ADS=1 FPS_SHOT_FRAME=300 FPS_SHOT=/tmp/glock.ppm ./build/game
```

`FPS_NOHUD=1` hides the HUD initially. `FPS_SHOT_FRAME` defaults to the existing 60-frame capture; 300 lets the pose transitions settle. J and V still toggle the HUD and third-person camera during normal play.
