# Five-storey Khrushchevka: plans, assets and exterior brief

Research checked 2026-10-09. This is a reference and acquisition pack, not an implemented building. No paid meshes purchased; no gameplay or rendering changes.

## Identification

The user's photograph shows Volodarskogo Street 28, Gatchina. Its [Wikimedia Commons source](https://commons.wikimedia.org/wiki/File:Гатчина._Улица_Володарского,_28.jpg) identifies it as **1-464A**. This is a useful series identification, not proof of the exact project suffix or measured dimensions of that address. The photographer is 3dfxVoodoo3; the image is CC BY-SA 4.0. It has not been copied into game textures.

Visible features to preserve: five floors; long rectangular body; rough light grey panel surfaces; clear vertical and horizontal panel joints; two window columns on the end wall; dark foundation/plinth; shallow pitched roof and dark eaves; projecting balcony stacks with a mixture of open rails, solid panels and resident-added glazing. Use the attached photograph as the exterior art direction. The unseen entrance elevation still needs reference matching.

## Plans acquired

1. **Original 1-464A assembly reference**, 37 PDF pages. [Source scan](https://files.stroyinf.ru/Data2/1/4293809/4293809682.pdf). Local: [PDF](references/khrushchevka/1-464a-assembly-reference.pdf), [section sheet preview](references/khrushchevka/1-464a-section-plans.png).
   - PDF page 4, general section sheet 1, has stairwell and apartment section schematics, dimension chains and a project/section table.
   - The recurring widths marked 260 and 320 are 2.6 m and 3.2 m structural bays. The sheet includes 14.2 m, 16.8 m and 19.4 m section lengths and 11.52 m depth examples. Choose one complete section family; do not mix unrelated section plans just to fit an exterior.
   - This is an assembly guide, not a complete room-level architectural working drawing set. It establishes the grid and stairwell pattern; doors and detailed partitions require additional tracing/reference.
2. **Lithuanian housing typology study**, 242 PDF pages. [Source](https://mmap.lt/wp-content/uploads/2023/06/bukletas-pages.pdf). Local: [PDF](references/khrushchevka/lithuanian-housing-series-reference.pdf), [elevation and plan preview](references/khrushchevka/1-464a-17lt-elevation-plan.png).
   - PDF page 78 / printed page 76 shows **1-464A-17LT**, five floors, four sections, 60 apartments, with an elevation and plan.
   - Drawing labels: 56.65 m length, 11.52 m depth, 15.05 m elevation height. These describe that Lithuanian variant, not the Gatchina photograph.
   - Useful secondary dimensional cross-check and Baltic context. Its roof and balcony treatment differ from the photo; do not copy that facade unchanged.

Both PDFs are kept as design references. No explicit redistribution license was verified for the scans/study; do not bundle them with released game assets. A search result presented [this scan](https://files.stroyinf.ru/Data2/1/4293784/4293784395.pdf) as 1-464A, but its own cover/title blocks say **1-467A-17**. It was excluded from the acquired pack.

## Mesh shortlist

| Candidate | Verified listing facts | Fit / remaining checks |
| --- | --- | --- |
| [Khrushchyovka Soviet Apartment Building With Modular Interior](https://www.fab.com/listings/3ed9e786-5f4d-490c-84d3-f79420917c3e?lang=ja), awesomeness3242 | Seller lists a repeatable five-floor section, two mirrored apartments per floor, switchback stairs, doors and balconies on floors 2-5; FBX and converted GLB/glTF/USDZ | Best functional candidate. Exact series, topology, triangle count, UVs, material count, price and selected license remain unverified. Seller's historical-accuracy claim is not independent validation. Two apartments per floor may not match our chosen 1-464A section. |
| [Khrushchevka](https://sketchfab.com/3d-models/khrushchevka-4c759da8b8914ec4924f68ee9cc3cbd5), quizzes | Free downloadable Russian panel house; approximately 2k triangles / 1.3k vertices; CC Attribution | Useful exterior/LOD candidate. Interior, actual openings and file contents unverified. Full page access was blocked; metadata came from indexed publisher listing. Attribution required; retain exact license with any eventual download. |
| [Five-story residential building. Khrushchyovka #4](https://3dsky.org/3dmodels/show/piatietazhnyi-zhiloi-dom-khrushchevka-4), vincen+ | Listing: $7, royalty free, 3ds Max 2020 + FBX, 33k polygons, 71 MB; 47 x 13.4 x 17.2 m; series 1-468B | Realistic panel exterior candidate, but different series. Archviz materials and pseudo-volume details need conversion. No fully traversable apartments promised. Listed polygons are not a verified runtime triangle count. |
| [Low Poly Soviet Khrushchyovka pack](https://www.fab.com/listings/cde6f1fd-c618-4ce8-8867-3cec06998d4a), Agor2012 | Blender/FBX/Unreal formats; accessible stairs and separate example apartment; faceted stylized art | Poor match for the requested realistic look. The apartment-loading Blueprint is not portable to our renderer. |

Prices are observations from research day, not purchase quotes. Meshes were not downloaded or inspected. No available asset was verified as an exact, fully enterable 1-464A match.

## Material acquired

[ambientCG Concrete034](https://ambientcg.com/view?id=Concrete034), 1K JPG archive, downloaded into the reference folder. Extracted Color, NormalGL and Roughness maps are in `models/textures/environment/khrushchevka/`. [License](https://docs.ambientcg.com/license/): CC0 1.0, permits modification and redistribution, including raw files in a game.

Use as a candidate for slabs, sills and foundation concrete. It is smooth concrete/plaster, not a complete match for the coarse pebble-like facade in the photo. The facade needs a separate coarse aggregate finish plus panel seams and restrained stains. The maps are not wired into the game; current world materials do not automatically consume a complete PBR stack.

## Recommended exterior approach

Author a modular **1-464A-inspired building** around a chosen real section plan. For the requested huge version, propose eight stairwell sections, five floors, approximately 110-115 m long and 11.5-12 m deep. This is a design proposal using roughly 14.2 m sections, not a reconstruction of the photographed address. Keep the length configurable. Do not upscale windows or floor heights to make it huge.

First milestone: exterior at real player scale with all four elevations, foundation, shallow roof, real window/door apertures, recessed frames, balcony slabs and rail/glazing variants. Start with one complete stairwell section and the end wall; approve their proportions before repeating. Reserve floor slabs, stair openings and apartment boundaries in the source even before interior art is finished. Avoid a solid box with painted windows that would have to be rebuilt for room access.

Near windows should reveal a little real depth and simple interior backing rather than flat black patches. Backing must be replaceable by actual rooms. Use deterministic balcony variations to reproduce resident alterations without randomly blocking future circulation. Model shell surfaces with thickness and separate doors/glass from structural walls.

## Game integration implications

- Existing `Mesh::create` supports authored vertex materials or UV layouts, but does not load FBX/GLB directly. Purchased formats need an offline export path. The soldier exporter does not preserve arbitrary Blender texture graphs. A textured building needs an appropriate building export/material path.
- Shared map collision currently uses at most 1,024 town boxes. A long building with all rooms and stair flights could exhaust that pool. Keep visible geometry separate from simplified shared collision; plan capacity and spatial queries before adding the full interior. Use the same wall/opening dimensions on the client and authoritative server.
- Prebuild geometry and buffers at startup. Batch repeating facade components/materials; cull by building section. Use near/medium/far LODs, dropping balcony clutter and railing detail at distance.
- Hundreds of transparent panes and rendering every furnished room from outside may add substantial overdraw and draw/shadow work. Start with simple glass and empty interior structure; measure before adding broad transparency or furniture. No frame-time estimate or performance result exists yet.
- Benchmark at matching camera, quality, drawable resolution and Release build. Record frame time in ms and percentage change. Shared deterministic placement will require a world revision update when integrated; wire changes require a protocol bump only if actually introduced.

Exterior acceptance: correct five-storey proportions at player scale; convincing facade/balcony rhythm from close and distant views; complete rear/side/roof treatment; openings aligned to reserved interior structure; collision consistent with structural surfaces; measured performance cost. Accessible rooms, stair traversal and multiplayer collision are the next milestone.
