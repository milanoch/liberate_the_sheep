# Natural oak fence — 2026-09-24–25

Current artwork, awaiting the user's visual review in Panda. The user rejected
the old rough two-rail fence, the whitewashed revision, and simply restoring the
old art. They requested that we choose a genuinely different, coherent design.

## Design and imported contract

One continuous natural-oak rail with softly chamfered edges and tidy rounded
post caps. The camera is strictly overhead; there are no tall front-facing
columns that rotate sideways on the vertical enclosure sides. This is new
built-in imagegen artwork, not a programmatic recolour or procedural mesh.

- Final source: `SourceArt/Environment/wood_fence_oak_source.png` (1916×821).
- First generated draft: `wood_fence_oak_front_draft.png`. Rejected internally
  because it showed broad front faces. Kept only as the input for the camera edit.
- Imported: `Assets/Textures/Environment/wood_fence_panel.png`, **1985×283**.
- Existing ID **1435494927**, .pmeta, material 0, all world/C++ bindings unchanged.
- Source visible alpha>8 bounds: (35,347)…(1879,484); crop 1853×146.
- Uniform scale 1.0712358337830545 to the original canvas, transparent padding
  above and below. The overhead caps are NOT stretched into tall rectangles.
- Imported alpha>8 bounds: (5,67)…(1980,215).
- Sprite quad ratio remains 7:1; section length, 6% overlap, positions, fence
  offset 2.6 cells and gate 2.2 cells are unchanged. Actual ink is thinner than
  old front-projected columns because these are now overhead top caps.
- Grass, sheep, movement, menus, gameplay and runtime material creation unchanged.

## Reproduce packaging

The helper's original two-path form remains a lossless crop. Optional canvas
dimensions perform only a uniform technical resample onto transparent RGBA,
preserving proportions and generated alpha. No repainting or colour selection.

```sh
swift SourceArt/Environment/prepare_pasture_assets.swift \
  SourceArt/Environment/wood_fence_oak_source.png \
  Assets/Textures/Environment/wood_fence_panel.png 1985 283
```

## Validation

- Helper executed in both modes. The old source in crop-only mode reproduces
  the original imported PNG byte-for-byte (git blob
  `d945c5a1fc91fb0b17f156d0c3f7fab326b2e337`).
- New imported SHA256:
  `16a6baf503ee8dfe8ae3466a716eb01ccb7b54a00c0bccac2d9941c2c5c42a5d`.
- Alpha is zero at canvas corners, at the upper/lower centre, and in the clear
  area above the rail. Rail-centre RGBA readback is (185,144,93,253) premultiplied
  sRGB. There is no opaque background/matte; generated edge alpha is retained.
- Existing offline preview compositor rendered actual level layouts 1/6/9 at
  1200×800 and level 9 at 540×960. Main and independent QA inspected small/large
  fields: corner joins, repeated post overlap, single rail and unobstructed gate.
  Detail naturally becomes subtle at small scale; silhouette/posts remain visible.
- SHA256 confirmed unchanged world, fence .pmeta, PastureScene, PenEscapePath,
  approved grass atlas and approved sheep atlas.
- No C++ changes, therefore no script-library rebuild needed.
- These are offline asset compositions, NOT screenshots of Panda and NOT final
  user acceptance. Only the human launches the editor/game. Stop Play, then
  **Reload (Cmd/Ctrl+R)**.

## Exact built-in generation prompt

```text
Use case: stylized-concept.
Asset type: production transparent 2D game sprite, ONE modular wooden fence section for an overhead sheep-pasture puzzle.
Primary request: design a refined, attractive NATURAL OAK pasture railing. This is a new design, not a recolour of a rustic two-rail fence. ONE single substantial continuous horizontal wooden rail with softly rounded/chamfered edges connects TWO neatly finished end posts. Calm intentional joinery, clean silhouette, warm pale-to-medium oat/honey oak top face, muted taupe-brown shaded thickness, very subtle broad woodgrain. Smooth well-maintained natural timber, not white paint, not weathered barn wood. Small chamfered post caps with clean bevels, no ornate metal hardware. Visually coherent with gently painted fluffy sheep and bright natural meadow grass.
Composition: isolated straight horizontal section, overall visible silhouette exactly about 7 times wider than tall. End post centres at 3.5% and 96.5% of the total object width; each post about 7% of total width and the full object height. The single rail is centred vertically and about 43% of the full object height; transparent clear space above and below the rail between the posts. Both ends symmetrical and connect cleanly when adjacent sections overlap by 6%. No middle post, no second rail, no slats, no diagonal brace.
Viewpoint: high overhead orthographic game view with a little bevel/thickness visible, NOT front elevation, NOT a diagonal isometric object; perfectly horizontal parallel edges. Broad top-facing timber surface reads clearly at tiny game scale. Soft nearly direction-neutral ambient lighting suitable for rotating this sprite 90 degrees. Only self-shading ON wood, no cast shadow on the transparent background, no black outlines.
Style: polished restrained painterly 2D game asset with believable wood volume; coherent broad colour areas, quiet surface detail. Not vector flat, not rough sketch, not photographic noisy grain, not glossy plastic, not oversaturated orange, not toy candy colours. No nails or bolts emphasized.
Output: wide landscape PNG with genuinely transparent alpha around the complete object; lots of empty transparent space above/below is fine. Preserve full end posts without clipping. Solid opaque wood, antialias edges. No ground, grass, scene, sheep, UI, text, logo, watermark, border or checkerboard baked into pixels. ONE finished sprite only.
```

## Exact built-in camera-correction edit prompt

Input was the first generated oak draft, NOT the rejected old/ivory artwork.

```text
Use case: precise-object-edit.
Input image 1 is the edit target: a newly designed one-rail natural oak fence sprite.
Primary request: correct ONLY the camera/viewpoint and corresponding visible faces so this asset reads as a TRUE STRAIGHT-DOWN ORTHOGRAPHIC TOP VIEW, as seen by a camera exactly overhead. Current image incorrectly shows long vertical front faces of the posts. Remove those front faces completely. Both end posts must read as flat rounded rectangular END-GRAIN TOP CAPS seen from directly above, with very subtle concentric end-grain lines; the entire visible post shape is the top cap, NOT a long upright column. The rail likewise shows its long TOP FACE, with only a tiny symmetric bevel around the edge, no broad dark front face. The result should resemble a carefully rendered PLAN VIEW of a fence, not its elevation.
Preserve the exact overall horizontal silhouette, transparency, rail thickness, end post positions, one-rail design and warm medium-light natural oak palette. Same two tall-in-image rounded rectangular post footprints at the two ends, now rendered entirely as their overhead top surfaces. No new parts. Rail stays one continuous horizontal beam between post centers. Slight bevel and soft wood texture, polished restrained painterly game art that remains readable when tiny.
No side-face projection, no perspective trapezoids, no isometric angle, no diagonal fence, no cast shadow on ground, no black outline, no white paint, no distressed rustic scratches, no grass, no background or text. Keep genuine transparent alpha around the whole object. Preserve full silhouette with no clipping.
```

The edit reduced projected post height despite the requested silhouette lock.
Uniform fitting deliberately preserves the more correct overhead square caps,
rather than stretching the art to the old front-elevation ink bounds.
