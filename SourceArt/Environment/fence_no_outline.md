# Oak fence: remove dark outline — 2026-09-30

Update 2026-10-02: a project-only gameplay workaround is now implemented in
`PastureScene` / `PastureFenceComposite.hpp`: the imported fence is alpha-blended
onto the existing opaque grass image before upload. See `fence_opaque_composite.md`.
It awaits user visual acceptance; ordinary edit-mode Sprite blending and the
underlying engine bug remain unchanged. The notes below are the prior diagnosis.

The user pointed out the black-looking border along the rail and post caps in
Panda. This is a texture-only edit of the current oak fence, not another redesign.

## Still unresolved: viewport alpha composition (2026-10-01)

The user's next screenshot still shows the dark border. RGB padding was correctly
imported, but was insufficient: the actual installed PandaEditor composites the
remaining scene alpha over the viewport panel rather than the grass behind it.

Read-only executable evidence:

- Renderer2D::flush at 0x100206ca4 sets 0x4, without alpha blending.
- RendererMetal::submit at 0x100297bf4 extracts blending from bit 0x10.
- Actual app default_unlit.frag writes sampled RGBA; tonemap.frag retains alpha.
- EditorLayer::syncPandaUIEditorViewport at 0x1000069fc–0x100006a64 passes the
  result native texture into ViewportView/ImageView.
- PandaUIImplMiren::render at 0x10011334c sets 0x10; its UI shader preserves alpha.
- MetalShader::getPipeline at 0x100294d4c–0x100294db0 uses source-alpha /
  one-minus-source-alpha for RGB and one / one-minus-source-alpha for alpha.
- GPU RGBA upload at 0x100293688–0x1002936b4 uses the source bytes directly;
  there is no hidden upload premultiply to correct in the PNG.

Correct engine blending/layering is the preferred fix. A fence-specific cutout
shader (discard small alpha, output alpha=1 otherwise) is a project workaround
with harder edges. Neither has been applied: editing the installed engine/SDK
requires user direction and matching current engine sources. No tests, builds,
game/editor launch or process attachment occurred. Do not report the PNG-only
changes below as a completed visual fix.

## Follow-up: filtered transparent RGB, not just painted shading

The user still saw the outline after the lighter artwork and an editor restart.
The PNG and its cooked `.ptex` had already updated, so repeating Reimport or
brightening the artwork was not the right fix.

Read-only inspection of the installed SDK found:

- `libPanda.a`: ordinary `Renderer2D::flush()` uses state `0x4`.
- `libMiren.a`: that enables depth; Metal blending requires state bit `0x10`.
- `default_unlit.frag` discards alpha <= 0.01, then outputs sampled RGB directly.
- The fence's linear/mip filtering blends opaque wood RGB with black RGB in its
  transparent padding. With this alpha-tested Sprite pass, those dark edge RGB
  values remain visible even when the painted wood itself has no dark outline.

The optional `--bleed-transparent-rgb` packer mode now exports straight-alpha
RGBA and extends the nearest solid-wood RGB into transparent/near-transparent
pixels (alpha <16). Seeds are alpha >=240, avoiding noisy colors in faint generated
edge pixels. All alpha values are retained; there is no opaque background and
no enlargement of the silhouette. Padding covers the full empty margin for
mip reduction, not only one pixel. The ordinary packer mode remains unchanged.

Repacked the same generated source into the same 1985×283 imported texture:
352691 transparent/near-transparent pixels received wood RGB. No new imagegen
pass, gameplay code, world, .pmeta or SDK changes. Only the necessary asset
packaging command was run; no tests, test builds, previews or editor/game launch.
The user still needs to judge the rendered result after Stop Play → Reload.

Continuation 2026-10-01: the editor's cooked `.ptex` (mtime September 30,
18:34:17) retains the padded straight RGB. Targeted pixel inspection found
(992,85) = RGBA(240,179,107,0) and (992,200) = RGBA(215,159,92,0), rather than
transparent black. The importer therefore did not undo the padding. No new
packing, tests, build or render run was needed; human visual acceptance is pending.

## Earlier artwork edits (insufficient by themselves)

- Tool: built-in imagegen, precise-object-edit (two targeted passes).
- First input: `Assets/Textures/Environment/wood_fence_panel.png`.
- Final generated source: `SourceArt/Environment/wood_fence_oak_no_outline_source.png`.
- Imported texture: `Assets/Textures/Environment/wood_fence_panel.png`.
- Existing canvas: 1985×283; asset ID 1435494927 and `.pmeta` stay unchanged.
- Keep the authored/runtime +70% transverse Sprite thickness, section placements,
  enclosure boundaries, gate width, sheep and grass unchanged. No C++/world edits.
- Replace dark edge shading with oak-colored shading. The first pass retained a
  dark lower strip, so a second pass specifically removes it and the cap/joint rims.
- Preserve transparent alpha via the uniform-fit asset packer. The follow-up
  adds invisible RGB padding, not a new painted design, alpha threshold or shadow.
- No tests, test files, verification builds, offline scene previews or engine/editor
  launches. User must inspect in Panda after Stop Play → Reload (Cmd/Ctrl+R).
  Texture changes do not require a script-library rebuild.

## Packaging

```sh
swift SourceArt/Environment/prepare_pasture_assets.swift \
  SourceArt/Environment/wood_fence_oak_no_outline_source.png \
  Assets/Textures/Environment/wood_fence_panel.png 1985 283 --bleed-transparent-rgb
```

## Initial edit prompt

```text
Use case: precise-object-edit.
Asset type: transparent modular 2D game fence sprite, strict orthographic top-down view.
Input image 1 is the EDIT TARGET, an existing horizontal oak fence section. Make one narrowly scoped edit: remove the dark/black contour and heavy dark bevel around the rail and both end-post caps. Replace those nearly black or dark-brown rim pixels with natural medium warm oak tones that blend into the neighboring wood. Keep a small, softly lit wooden bevel, but no dark continuous stroke anywhere, including the lower rail edge, joints, cap edges and outer corners. There must be NO cast shadow outside the wood and NO black outline or dark halo.
Preserve the existing single rail, two end-post caps, exact straight-down camera, complete silhouette, proportions, total section length, rail thickness, cap sizes and positions, grain texture and overall oak color. Do not redraw as a different fence, add rails/posts, make it white, or change the viewpoint. Keep the same long narrow composition centered vertically, with clear transparent padding above and below.
Output: edited fence sprite alone on genuinely transparent RGBA background, not black/white/checkerboard matte. Solid opaque wood with antialiased edges. No grass, scene, sheep, UI, labels, lettering or watermark. Preserve the entire object without clipping.
```

## Targeted refinement prompt

```text
Use case: precise-object-edit.
Input image 1: edit target, the same oak fence sprite after an initial rim-lightening pass.
Required single correction: the horizontal bottom edge of the beam STILL HAS A DARK BROWN CONTINUOUS STRIPE. REMOVE THAT ENTIRE STRIPE. Paint it as medium-light golden oak matching the beam's face, NOT a darker bevel. Likewise remove the dark lower/right rims of both post caps and the two dark vertical joint creases. Any remaining dark perimeter contour should become the same softly textured medium-light wood as the adjacent face. This is removal of baked shadow/outline, not merely a thinner black outline.
The finish should be softly flat-lit natural timber all the way to the transparent edge: NO outline, NO dark border, NO deep brown edge, NO ambient-occlusion strip, NO cast shadow, NO black under-edge, NO dark crease at rail/cap junctions. Small wood-grain color variation remains, but the silhouette is defined only by alpha. Keep the entire exact silhouette, length-to-thickness ratio, single beam, square rounded end caps, positions, soft golden-oak palette and existing grain. No new components, no changes of perspective. Truly straight-down orthographic.
Preserve transparent background and output RGBA with genuinely transparent empty space. Center the complete fence horizontally with a little clear padding on the left/right so neither cap touches the canvas edge. No grass, ground, text or other content.
```
