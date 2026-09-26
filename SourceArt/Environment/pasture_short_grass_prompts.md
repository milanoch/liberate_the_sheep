# Short pasture grass and flock wear — 2026-09-22–23

**Historical/rejected version.** The user found seams and black puddle outlines
in the actual Panda viewport. Current artwork, opaque single-surface rendering
and updated packer/preview commands are in [opaque ground](pasture_opaque_ground.md).
Do not use the old packing commands below with the current opaque-only packer.

Created with the built-in image_gen tool, not the CLI. Gameplay only; menu art,
fence and accepted sheep motion are unchanged.

## Assets and preparation

- Imported atlas: `Assets/Textures/Backgrounds/pasture_grass.png`.
- Existing editor-issued ID **997711114**, same .pmeta, unchanged sRGB/filtering.
- Atlas: **2048×1024**, **2 columns × 1 row**, 1024×1024 cells.
  Index 0 = opaque short grass; index 1 = irregular transparent worn soil.
  One row also avoids Panda's nonstandard atlas-row modulo addressing.
- Source images: `pasture_short_grass_source.png`, `pasture_wear_source.png`.
  The September 12 `pasture_grass_source.png` remains as historical source.
- `pack_pasture_grass.swift` only resizes/packs generated images, retaining alpha.
  The worn source is fitted to 976×976 with a 24px transparent gutter. No
  threshold mask, procedural grass painting, materials or runtime textures.

Reproduce from repository root:

```sh
swift SourceArt/Environment/pack_pasture_grass.swift \
  SourceArt/Environment/pasture_short_grass_source.png \
  SourceArt/Environment/pasture_wear_source.png \
  Assets/Textures/Backgrounds/pasture_grass.png
```

## Scene and scale

`game.world` contains an editable `PastureGround` group with 384
`PastureGrassTile` and 96 `PastureWornPatch` Sprite slots. Only needed slots
are visible. They reference the imported atlas directly; no objects are
created for grass at runtime. Existing GameBackground is the underneath
fallback, now referencing atlas cell 0.

Each grass tile covers **6 board cells**; a sheep is about 2 cells long.
Fine blades therefore stay much smaller than sheep at every level size.
The tile grid is anchored to the board, independent of window aspect.
The texture repeats without reflection: mirrored tiling was rejected in the
offline preview because it created artificial diamond-shaped patterns.
A tiny geometric overlap plus two screen pixels of raster padding hides filtering fringes from the
adjacent transparent atlas cell. All ordinary desktop/phone test layouts
use 6-cell tiles; pathological ultrawide/tiny viewports can enlarge tile pitch
uniformly to fit the finite authored pool without leaving empty strips.

`PastureWear` copies initial occupied cells when a level loads. A smooth
2.4-cell-radius local kernel measures crowding, independent of sheep order.
Irregular seeded sites become more worn in dense neighborhoods; isolated
sheep do not make automatic circular bald spots. Rotations and slight
aspect variants disguise repeated decal silhouettes. The surface remains
green outside the flock. Patches stay after sheep leave, including across
resizes, and are recaptured only on level load/restart. This is persistent
visual wear for the current level, not a time-based grass growth simulation.

All ground layers are behind sheep and fences. No clickable cell grid,
new collision geometry, changes to escape routes, or menu edits.

## Offline visual check

This composes the actual imported sprites with real generated boards, density
snapshots, coats, layout and fence placement. It does not launch PandaEditor or
the game, and does not reproduce engine postprocessing or draw the HUD. The HUD's
reserved layout space is included. Checked levels 1, 6 and 9 at 1200×800 and
level 9 at 540×960. Grass stays small; wear clusters around the initial flock.

After a test-enabled Debug build:

```sh
Scripts/liberate_the_sheep/bin/liberate_the_sheep_export_pasture /tmp/liberate-pasture-preview.json
swift SourceArt/Environment/preview_pasture.swift /tmp/liberate-pasture-preview.json "$PWD" /tmp/liberate-pasture-previews
```

## Generation prompts

### Dense short grass (new image, photorealistic-natural)

```text
Use case: photorealistic-natural.
Asset type: seamless repeating albedo ground texture for a top-down 2D sheep pasture game, square 2048x2048.
Primary request: a genuinely realistic dense SHORT grazed meadow lawn viewed vertically straight down. Fine narrow grass blades 2–5 cm long, tightly packed, naturally interwoven, subdued fresh olive green with straw-green tiny highlights, slight organic variations, healthy but farm-real and not plastic golf-course turf. The image covers about THREE metres square of ground; individual grass blades are very small, not enormous leaf tufts. Photographic texture quality.
Composition: entire frame uniformly occupied by grass, orthographic plan view, no perspective or horizon. Seamlessly tileable horizontally and vertically: identical visual density, colour, illumination at all four borders, no vignette or border gradients. No large identifiable clumps, distinct landmarks, large dark islands, cracks, flowers, broad leaves, rocks, animals, fences or objects.
Lighting: uniform diffuse overcast daylight, restrained contrast and saturation, absolutely no directional long shadows or lighting gradient.
Opaque background, no text, no grid, no frame, no checkerboard. Ground surface texture only, not a landscape picture.
```

### Worn patch (edit using the generated grass as reference)

```text
Use case: precise-object-edit.
Asset type: transparent feather-edged ground-wear decal for a straight-down 2D sheep pasture game.
Input image: supplied grass texture is the edit target and material reference.
Change the middle of this patch into realistically worn, grazed down, compacted meadow soil, with a few remaining tiny short grass blades. Dry muted brown-earth and olive ground, soft cloudy variations, tiny earthy crumbs, not cracked desert. Keep the reference's orthographic overhead view, uniform diffuse lighting, realistic photographic microtexture, and small scale of grass, though reduce the dense spaghetti of pale grass stems. There should be approximately 60 percent exposed soil in the central region and a scattering of short grass.
CRUCIAL: this is a flat terrain overlay, NOT a lifted island or cut-out sod clump. No raised edge, rocks, large tufts, border, thick silhouette or cast shadow. Irregular soft wispy organic outline, broad gentle fade into genuinely transparent alpha around its outside. The ground density fades gradually from opaque near the central earthy regions through semi-transparent isolated tiny sparse grass and soil flecks to fully transparent edges. Keep a transparent margin of at least 12 percent on every side. Avoid circle/oval/geometric shape; use an asymmetric low-contrast worn area with three soft irregular lobes. Square image. Absolutely no text, checkerboard drawn into pixels, border, lighting gradient, perspective, animals or objects. Output with actual transparent background.
```
