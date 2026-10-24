# Cartoon pasture grass — 2026-10-01

The user requested illustrated/cartoon grass instead of the photographic,
visibly repeating material. They explicitly approved the existing worn/bald
areas and asked to preserve them.

## Follow-up: clearer blades and softer worn edges

The user liked the new cartoon style but found it blurry, with an abrupt
grass-to-earth transition. The artwork/atlas is not regenerated or repacked.
Only `PastureSurface.cpp` changes in this follow-up:

- The four hashed grass samples retain their UVs and smooth blend weights.
  To counter contrast loss when independent blades are averaged, compute the
  RGB mean of grass cell 0 once and restore contrast around it with gain
  `1 + 0.7 * (1 / sqrt(sum(weight²)) - 1)` (range 1–1.7). No nearest-sample
  switching, sharpened patch boundaries, extra texture taps or noise are added.
- The composed ground requests up to two texels per logical UI point, so it
  need not enlarge a one-texel-per-point bitmap on Retina. The existing 2048
  maximum side and 2,097,152-pixel budget still apply; world-space blade size
  and the one-Sprite ground remain unchanged.
- For density `d` including the existing broad variation, retain the old mask
  `smooth((d - 0.15) / 0.70)` in the core. Blend toward `smooth(d / 0.85)` only
  at the outer edge, with strength `0.65 * (1 - smooth(d / 0.60))`. This reveals
  small amounts of earth through sparse grass before the old cutoff. At
  `d >= 0.60` the original mask is unchanged; at zero density there is no earth.
  The original density snapshot, soil source/sampler, and 0.78 soil cap remain.

The working `liberate_the_sheep` library was rebuilt successfully (only that
target). No tests, verification builds, previews, editor or game were run.
After Stop Play → Reload, the user should judge blade clarity and the softer
outer falloff, especially on level 9. Visual approval is still pending.

## Artwork and installation

- Generated with the built-in `imagegen` tool, using the exact prompt below.
- Source: `SourceArt/Environment/pasture_cartoon_grass_source.png`.
- Original generation output:
  `/Users/milan/.codex/generated_images/01a03de4-d2db-7173-a871-84b0596ed643/exec-e8395dcc-3702-414b-b171-c2b8ee71f201.png`.
- Dense grass only: small painted blades, no baked dirt holes or patch borders.
- The existing `pasture_brown_soil_source.png` is reused unchanged.
- Runtime atlas: `Assets/Textures/Backgrounds/pasture_grass.png`, 2048×1024,
  dense grass on the left and approved soil on the right, fully opaque.
- Existing editor-issued asset ID **997711114** and `.pmeta` are not edited.
- The previous photographic source is retained for reference.

Applied with the existing asset packer (not a test or scene preview):

```sh
swift SourceArt/Environment/pack_pasture_grass.swift \
  SourceArt/Environment/pasture_cartoon_grass_source.png \
  SourceArt/Environment/pasture_brown_soil_source.png \
  Assets/Textures/Backgrounds/pasture_grass.png
```

## Grass sampling only

`PastureSurface.cpp` no longer uses the three-cell repeating sampler for dense
grass. Four overlapping source patches are blended with smooth weights;
source offsets are hashed from absolute signed grid coordinates and the existing
level seed. Grid spacing is 1.5 cells, while each source sample keeps the original
six-cell detail scale (about three sheep lengths per source square).
The patches neither wrap nor mirror; adjacent patches share their boundary
samples, without introducing a fixed three/six-cell repetition.

In the initial cartoon revision the soil sampler, density/wear snapshot, broad
mask variation, transition formula and soil cap of 0.78 were unchanged; the
follow-up above softens only the outer transition. The new grass still
contributes to the mixed worn pixels, but the approved soil source and shape of
the worn areas are preserved. The single opaque background Sprite, color-space
conversion and allocation caps are unchanged; no decals or extra scene objects
are added. Sheep, fence, menu and gameplay are outside this change.

Only the working script-library target is built to apply the C++ change:

```sh
cmake --build Scripts/liberate_the_sheep/cmake-build-debug \
  --target liberate_the_sheep --parallel 4
```

No tests, test targets, verification builds, offline previews, editor or game
are run. Visual acceptance remains with the user after **Stop Play → Reload
(Cmd/Ctrl+R)**: inspect the cartoon style, blade/sheep scale, lack of repeating
blocks, and the preserved soft brown worn areas, especially on dense levels.
The separate fence alpha-blending issue remains unresolved.

## Exact generation prompt

```text
Use case: stylized-concept.
Asset type: production square seamless opaque top-down grass albedo texture for a friendly 2D cartoon sheep puzzle game.
Primary request: a beautiful softly hand-painted short green pasture, distinctly cartoon/illustrated rather than photographic. A quiet, clean, gently painted mid-green turf foundation with many small expressive tapered grass strokes and occasional tiny two- or three-blade tufts. Soft rounded brushwork, restrained lighter yellow-green tips and subtle cooler green accents, no ink outline, no harsh shadows. Friendly polished storybook/mobile-game environment art, not pixel art and not a flat vector icon pattern.
Scale matters: the whole square represents roughly the length of THREE adult sheep end-to-end. Most visible blades must be very short, about 8–18 pixels long in a 1024x1024 texture, and tufts no more than 24–30 pixels across. Approximately 120–180 unevenly scattered tiny blade groups amid quieter painted turf; no tall grass, bushes or giant clumps. Clear readable shapes instead of photographic gritty micro-noise.
Composition: perfectly vertical orthographic bird's-eye material swatch filling the entire square, seamless/tileable in both directions. Balanced near-uniform density and brightness across the image, organically irregular placement, no stamped duplicates or rows. Keep broad color variation extremely gentle so it does not form repeating blotches, checkerboards, diamonds, islands or tile borders. No distinctive central feature or directional lighting.
Palette: pleasant natural spring/moss greens around warm medium #78AD52, light tips around #A5C56B, restrained darker strokes around #648F48. Not neon lime, not muddy olive, not teal. The grass should complement cream/brown/charcoal sheep and warm light-brown soil.
IMPORTANT: this is ONLY the dense grass material. Do NOT draw bare earth, dirt holes, worn paths, bald patches, mud, tracks, rocks, flowers, fallen leaves, fence, sheep or any other objects. The game already creates approved brown worn areas separately and those must not be baked into this texture.
Output: one square full-bleed opaque RGB/RGBA PNG, 1024x1024 or larger square, every pixel fully opaque. No transparency, margin, vignette, border, horizon, text, labels or watermark.
```
