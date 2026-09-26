# Gameplay pasture artwork — 2026-09-12

Generated with the built-in `image_gen` tool (not a CLI or external art pack).
Both are gameplay assets; no menu art was replaced.

## Grass

Historical September 12 version. The imported grass asset now uses the
[September 23–24 opaque ground sources](pasture_opaque_ground.md);
the original source below is retained for reference.

Final asset: `Assets/Textures/Backgrounds/pasture_grass.png`.
Source: `SourceArt/Environment/pasture_grass_source.png`.

```text
Use case: stylized-concept
Asset type: opaque fullbleed tileable grass texture for an orthographic top-down mobile sheep puzzle game.
Primary request: a fresh green pasture lawn viewed straight down, hand-painted 2D game art with fine natural grass blades, a few tiny unobtrusive clover tufts and soft irregular grass color variation.
Composition: the entire square image is grass, same detail density and brightness all the way to all four edges, seamless-looking edges. Small-scale surface details only; calm clean reading behind sheep. The image represents approximately 18 meters of ground across, so individual blades and clover are very small.
Lighting: soft even daylight, no directional shadows or hotspots.
Palette: lively but restrained sage and meadow green, natural sunny lawn, no dark olive mud and no neon green.
Constraints: opaque image, no transparency, no border, no vignette, no central empty oval, no visible rectangular patches, no checker pattern, no grid, no path, no dirt, no sheep, no fence, no trees, no flowers, no rocks, no sky, no lettering, no UI, no watermark. This is a production ground texture, not a landscape illustration.
```

## Fence panel

Historical September 12 brown artwork; the current imported asset uses the
[September 24–25 single-rail oak design](fence_oak.md). The
[ivory redesign](fence_ivory.md) and restoration of the old brown art were both
rejected. Canvas, scene placement and texture ID remain unchanged. The source
and reproduction commands below describe the OLD artwork, not the active one.

Final asset: `Assets/Textures/Environment/wood_fence_panel.png`.
Source: `SourceArt/Environment/wood_fence_panel_source.png`.

```text
Use case: stylized-concept
Asset type: a single modular fence-section sprite for a 2D top-down sheep pasture game.
Primary request: ONE straight rustic wooden farm fence section, horizontal from left to right, seen from a VERY HIGH overhead angle almost directly from above. It must match top-down animals on a ground plane, NOT be a tall fence seen from the front.
Subject: two slender parallel weathered honey-brown wooden rails, connected to a square-cut stout end post on the left and another matching end post on the right. Clear wood grain, charming painted 2D natural game art, clean detailed silhouette. Visible TOP faces of the wooden rails and post caps dominate. Low fence proportions. Panel silhouette approximately SIX times wider than its total visible height; posts compact, short projected side faces only. Whole object, level horizontal rails. Symmetric perpendicular projection with no vanishing point, no diagonal/isometric rotation.
Composition: center exactly one panel on a genuinely transparent background. Generous transparent padding outside the wooden object. Transparent open gap between both rails too. No grass below and no cast shadow beyond the wood.
Lighting: softly shaded wood, gentle overhead light, similar brightness on both ends so repeated copies join.
Constraints: real alpha transparency, no checkerboard baked in, no solid background, no ground, no scenery, no animals, no grass, no stones, no flowers, no extra loose posts, no diagonal brace, no gate, no fence enclosure, no text, no logo, no watermark. This is an isolated modular production sprite, not a scene.
```

The source has genuine alpha, including the open space between rails. The
packaging script only removes unused transparent margins, preserving all pixels
and the source alpha inside the crop:

```sh
swift SourceArt/Environment/prepare_pasture_assets.swift \
  SourceArt/Environment/wood_fence_panel_source.png \
  Assets/Textures/Environment/wood_fence_panel.png
```

PNG measurements: grass 1254×1254, opaque; fence source 2172×724; packaged fence
1985×283. Visible alpha > 8 bounds in the packaged fence: (4,4)..(1980,278).
The post centers are approximately 3% and 97% of visible panel length. Adjacent
panels should share/overlap their end posts, rather than leave transparent seams.
Use the original aspect ratio: a 2.4-cell-wide section is about 0.34 cells high.
All resource registration and `.pmeta` creation are left to PandaEditor.

The image viewer may exaggerate colored speckles in almost-transparent edge
pixels. A premultiplied-RGBA inspection found no red/yellow fringe at alpha > 8;
all such pixels have alpha at most 2/255 (less than 1% opacity). Normal Sprite
alpha blending is required, as it is for the sheep sprites. The rail gap center
has alpha exactly zero. A targeted imagegen matte-cleanup variant was inspected
and discarded because it did not improve the source.
