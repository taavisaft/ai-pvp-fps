# Keila bake

Source: Maa- ja Ruumiamet open data (attribution required). Raw files live in `_RESOURCES/keila/raw/` (git-ignored).

Centre: Keila Kultuurikeskus, Keskväljak 12 — L-EST97 E 524102.52, N 6574626.33. Game x = E − E0, z = −(N − N0).

```sh
cd _RESOURCES/keila/raw
for s in 63721 63723; do curl -o ${s}_dtm_1m.tif "https://geoportaal.maaamet.ee/index.php?lang_id=1&plugin_act=otsing&kaardiruut=$s&andmetyyp=dem_1m_geotiff&dl=1&f=${s}_dtm_1m.tif&page_id=614"; done
BB="523028,6573552,525177,6575701"
for l in e_401_hoone_ka e_501_tee_a e_501_tee_j e_502_roobastee_j e_203_vooluveekogu_a e_203_vooluveekogu_j e_202_seisuveekogu_a e_305_puittaimestik_a e_305_puittaimestik_p e_302_ou_a e_304_lage_a e_405_piire_j e_301_muu_kolvik_a e_303_haritav_maa_a; do curl -o $l.json "https://gsavalik.envir.ee/geoserver/etak/wfs?service=WFS&version=2.0.0&request=GetFeature&typeNames=etak:$l&outputFormat=application/json&srsName=EPSG:3301&bbox=$BB,EPSG:3301"; done
cd ../../.. && python3 tools/keila/fetch_ortho.py && python3 tools/keila/bake.py
```

Needs Python 3 + Pillow. Writes `src/keila_data.cpp`, `textures/keila_surface.png` (R paved, G gravel/rail, B water), `textures/map_keila.png`, `textures/keila_ortho_near.jpg` (±512 m, 25 cm/px) and `textures/keila_ortho_far.jpg` (±1024 m, 1 m/px) from the Maa-amet `EESTIFOTO` WMS, `_RESOURCES/keila/preview_north_up.png`. Bump `NET_WORLD_REVISION` after a rebake that changes heights, buildings or spawns.

## Landmarks (textured buildings)

`landmarks.json`: one entry per building, matched by ETAK `ads_lahiaadress`. `walls` = one string per footprint wall, in ring order (bake prints the needed count). One letter = auto-fill 3 m bays: `W` windows, `D` windows + door/stair bay, `S` stair glazing, `C` cladding, `P` plaster. Several letters = explicit bays, left to right as seen from outside: `A` window+panel, `B` window, `C` cladding, `P` plaster, `D` door with stair glazing above, `S` stair glazing.

Texture = 2048×1024 atlas, 8 cells of 512²: row 0 window+panel, window, cladding, plaster; row 1 plinth, plinth window, door, stair glazing. One cell = one 3 m bay × one floor. `facade_pae7.py` authors the Pae tn 7 atlas; replace cells with own rectified photos to go photo-real. No Street View / third-party imagery.
