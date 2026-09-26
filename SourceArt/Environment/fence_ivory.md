# Ivory timber fence — 2026-09-24

**Rejected by the user; rolled back on 2026-09-24.** The original brown
`wood_fence_panel.png` is restored byte-for-byte from the pre-change backup.
This document and the ivory source are historical only. Do not reinstall this
variant without a new user request. The temporary brown restoration was also
rejected; the active replacement is now the [single-rail oak fence](fence_oak.md).

The user approved the current grass and said the fence size is correct, but
requested a different appearance. This change replaces **only the fence artwork**:
light whitewashed/ivory timber, warm wood showing through worn edges, clearer
bevels, two continuous rails, square end posts and small nail heads.

## Saved assets

- Source: `SourceArt/Environment/wood_fence_ivory_source.png`.
- Imported replacement: `Assets/Textures/Environment/wood_fence_panel.png`.
- Built-in imagegen edit, using the old imported PNG as footprint reference.
- Existing texture ID **1435494927**, .pmeta and material 0 unchanged.
- Original `wood_fence_panel_source.png` is retained for restoring the old art.

The generated transparent source is 2171×724. The existing packaging helper
crops unused alpha margins to 2163×314, then a technical resample restores the
exact original imported canvas **1985×283**. No world, mesh, material, script
or runtime layout change is needed.

```sh
fence_art_dir=$(mktemp -d /tmp/liberate-fence-art.XXXXXX)
swift SourceArt/Environment/prepare_pasture_assets.swift \
  SourceArt/Environment/wood_fence_ivory_source.png "$fence_art_dir/cropped.png"
sips --resampleHeightWidth 283 1985 "$fence_art_dir/cropped.png" \
  --out "$fence_art_dir/rejected_ivory_preview.png"
```

## Preserved contract and checks

- Same 7:1 Sprite quad, fence offset 2.6 cells, gate width 2.2 cells.
- Same 80 authored fence Sprite slots, same positions/rotations and 6% panel
  overlap. No change to sheep paths, ground appearance or animation.
- Visible alpha>8 bounds (3,3)…(1980,278); previous (4,4)…(1980,278):
  footprint differs at most one source pixel.
- Post-cap centers 3.73% / 96.19%; previous 3.65% / 96.28%.
- Center rail gap is fully transparent; wood interiors approximately
  alpha 252–253/255. No dark semitransparent matte was found.
- Saturated edge specks exaggerated by the standalone image viewer have
  alpha at most 3/255, as in the old asset; do not mistake them for solid borders.
- Actual scene composition inspected offline at level 1 (1200×800) and level 9
  (540×960), including shared posts and 90-degree corners. These are not engine
  screenshots: only the human launches the game.
- Checksums confirm world, fence metadata, layout/path scripts, accepted grass
  and sheep atlas are unchanged by this task. C++ was not changed or rebuilt.
- Reload the imported texture with Stop Play → Reload (Cmd/Ctrl+R).

## Exact built-in imagegen edit prompt

```text
Use case: precise-object-edit.
Asset type: redesigned modular wooden fence Sprite for a cozy top-down sheep pasture game.
Input image: current fence panel is the edit target and footprint/layout reference, NOT a style to copy.
Primary request: make this fence much more attractive and readable on green grass. Redesign it as a charming, well-crafted country fence in lightly WHITEWASHED warm ivory wood, with honey-coloured timber showing at occasional worn edges. Broad clean timber faces, softly rounded bevels, restrained natural woodgrain and a couple of small dark metal nail heads. Attractive subtle 3D volume painted into the sprite, friendly polished 2D game illustration matching fluffy painted sheep. Not old dark orange wood, not thin wires.
Keep the exact narrow horizontal footprint: ONE section roughly SEVEN times wider than its overall height. A sturdy squared end-post at each extreme and two continuous straight horizontal wooden rails between the posts; make the upper rail visually broad and clear. Both post centres at about 3% and 97% of the visible total width so consecutive sections share a post. Do not add any intermediate post, diagonal brace, gate, grass or extra item.
Camera: almost directly overhead, visible top surfaces and only a little thickness on the lower edge. Keep parallel geometry and the current view direction. NOT a front-elevation picket fence and NOT an isometric diagonal. Entire object horizontal, same scale and bounding proportions as the input.
Lighting: soft neutral overhead illumination, light warm top faces and gentle medium brown self-shading only on the wood; no black outline, no heavy near-black areas, no shadow cast onto invisible ground, no semitransparent halo. Solid opaque wood interiors.
Genuinely transparent alpha around the object AND in the long gap between rails. Clean antialiased silhouette. Transparent margin around all sides. No checkerboard painted into pixels, no background, no text, no logo. Change artwork only, preserving the connection points, orientation and narrow footprint.
```
