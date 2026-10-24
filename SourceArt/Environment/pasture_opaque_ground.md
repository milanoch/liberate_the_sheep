# One opaque pasture surface — 2026-09-23–24

Update 2026-10-01: dense grass is now cartoon artwork with nonperiodic patch
sampling; the approved soil and wear calculation remain unchanged. See
`pasture_cartoon_grass.md` for the current source and packing command. The
photographic source, periodic grass sampler, checks and commands below describe
the historical September version. Do not run those tests/previews without an
explicit user request (standing instruction since 2026-09-30).

The user rejected the September 22–23 Sprite tiles and transparent decals:
visible seams, dull grass, and black puddle-shaped outlines in the actual
Panda viewport. The earlier CoreGraphics preview did not reproduce the
engine's alpha/material rendering. Do not reintroduce those overlay sprites.

## Artwork and imported resource

Both new source images were generated with the built-in imagegen tool:

- `pasture_fresh_grass_source.png`: brighter short green grass.
- `pasture_brown_soil_source.png`: light warm brown earth with sparse tiny grass.

They are full opaque material samples, **not separate bald-patch images**.
`pack_pasture_grass.swift` resizes/packs them into
`Assets/Textures/Backgrounds/pasture_grass.png` (2048×1024; 2×1 cells).
Both cells have alpha 255 everywhere, with no transparent gutter.
Existing editor-issued ID **997711114** and .pmeta remain unchanged.
Old source images are retained for reference, not used by the current ground.

The authored GameBackground Sprite remains in game.world. The previous
384 grass-tile Sprites, 96 wear-decal Sprites and their container were removed;
91 original scene entities remain. Fence, sheep, paths, menu and UI are unchanged.

## One image per level/layout

`PastureSurface.cpp` combines the imported materials with the **continuous**
local density of the initial flock, rather than placing patch silhouettes.
It creates a single complete image covering the visible camera rectangle;
the Sprite uses one full texture cell at runtime. Empty areas remain grass,
crowded areas reveal warm earth through a smooth falloff. Broad seeded variation
only modulates the density mask; it does not paint replacement artwork.
The initial snapshot persists after sheep leave, including across window resizes.

A six-cell source sampling footprint keeps the original blades small compared
with an approximately two-cell sheep. Periodic cross-fading of four shifted
source samples removes edge discontinuities without reflection/kaleidoscope
patterns. The combined sampling repeat is three cells (half the footprint).
There are no adjacent ground quads, alpha overlays, dark outline masks, or
mipmap boundaries in the final image.

Composition happens on successful level load/restart and viewport resize,
not every animation frame. Output is aspect-preserving, at most 2048 pixels
on either side and at most 2,097,152 pixels (8 MiB RGBA).
A cached source atlas is retained; the generated GPU texture is reused when
dimensions agree and deleted on resize replacement/shutdown.

This runtime-composited texture is an intentional response to the user's
request for one density-dependent ground image. Its source artwork is still
an imported project resource and its Sprite is authored in the world.
Do not turn it back into runtime scene objects, meshes or separate decals.

## Panda colour/lifetime contract

Current shipped backend audit: `createTextureRGBA8` produces linear
RGBA8 UNORM, clamp/linear filtering, no mipmaps. `readPixelsRGBA8` returns
raw sRGB bytes from the imported cooked asset, not a runtime GPU readback.
The composer/preview output is sRGB with every alpha byte exactly 255.
`PastureColorSpace.hpp` decodes RGB once before the linear GPU upload;
alpha stays 255. This matches the imported source's colour-space semantics.

Save the imported source handle before replacing the Sprite binding.
On shutdown restore it (atlas cell 0, 2×1), then delete only the owned runtime
texture. Source-read/composition failures keep the imported grass fallback
and log a Reload warning. Row 0 of the composed bitmap corresponds to world top.

## Reproduce and inspect without launching the game

Run a Debug build with tests enabled, then:

```sh
pasture_check_dir=$(mktemp -d /tmp/liberate-pasture-check.XXXXXX)
swift SourceArt/Environment/pack_pasture_grass.swift \
  SourceArt/Environment/pasture_fresh_grass_source.png \
  SourceArt/Environment/pasture_brown_soil_source.png \
  "$pasture_check_dir/pasture_grass.png" "$pasture_check_dir/pasture_grass.rgba"
Scripts/liberate_the_sheep/bin/liberate_the_sheep_export_pasture \
  "$pasture_check_dir/pasture_grass.rgba" "$pasture_check_dir/preview"
swift SourceArt/Environment/preview_pasture.swift \
  "$pasture_check_dir/preview/preview.json" "$PWD" "$pasture_check_dir/preview"
```

The exporter now calls the actual single-surface compositor; it does not imitate
it with Sprite overlays. It composes levels 1/6/9 at 1200×800 and level 9 at
540×960 with the actual generated flock, coats, fence and HUD layout reserve.
This remains an offline visual check: no engine postprocessing/HUD is drawn,
and only the human launches PandaEditor/game.

Debug build and all 10 CTest passed (14.44 seconds). Added tests check
opaque alpha, warm-brown crowded areas, smooth density/texture joins even
for deliberately nonmatching source edges, initial-flock persistence,
world/resize anchoring, allocation caps, and sRGB-to-linear upload conversion.
Repacking matched the installed PNG SHA256:
`200a74830696b7b66ffb42fb0e028cd675755d2998e3bc2df572fcdd29282301`.

## Exact generation prompts

### Fresh grass

```text
Use case: photorealistic-natural.
Asset type: opaque square albedo texture for ground in a top-down 2D sheep game.
Primary request: fresh cheerful lively GREEN closely grazed pasture grass, brighter and greener than a dull olive lawn. Photorealistic fine short grass viewed vertically from above under bright soft daylight. Very small blades, approximately 2–4 cm, image covers THREE metres square. Dense healthy grass, some slightly lighter tips and tiny gaps, real soft natural variation. Think fresh spring meadow, not artificial turf, not moss, not tangled pale spaghetti.
Composition: flat evenly lit entire square grass surface. No horizon, perspective, shadows, objects or raised clumps. Uniform average illumination from edge to edge, same detail and colour at all borders for seamless repetition. No dark patches or central feature.
Palette: warm fresh medium leaf-green and light grass-green, lively without neon saturation; avoid gray, brown wash, dark olive, white straw and black crevices.
Photographic texture not cartoon. Opaque RGB background everywhere, NO transparency, vignette, border, grid, rocks, flowers, fence, sheep, text or watermark.
```

### Brown soil

```text
Use case: photorealistic-natural.
Asset type: square OPAQUE ground albedo texture for a straight-down 2D sheep pasture game.
Primary request: natural LIGHT WARM BROWN compacted meadow earth where sheep have grazed away most grass. Full frame is the same flat soil surface, with only a few tiny short green grass shoots and dry roots scattered across it (about ten percent grass). Fine crumbly dry earth texture, soft warm ochre-brown and tan variation, subtle organic grain.
Scale: photograph of a THREE metre square area from exactly vertically above; grains and grass shoots are tiny. It should read as comfortable ordinary farm soil, not mud puddles or a crater.
Lighting: bright diffuse daytime, uniform edge to edge, no shadows, no dark ring or black outlines. Relatively bright earth, NOT black/gray/charcoal dirt, no water, no wet shine, no cracked desert.
No patch shape, no silhouette, no transparent background, no cut-out or floating island, no frame, no border or vignette. Entire image is filled by opaque soil, same average hue at all four edges for seamless use. No rocks, flowers, fence, animals, footprints, text, UI or watermark.
```
