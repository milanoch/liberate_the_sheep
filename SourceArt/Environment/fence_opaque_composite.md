# Rigid fence alignment and opaque edge composition — 2026-10-02

The user found the outward-wandering fence line unnatural, like a wavy pencil
stroke, and asked again to remove all black alpha edges. This revision changes
placement and rendering, not the accepted wood/grass artwork or game rules.

## Latest correction: straight rails joined at shared posts — 2026-10-04

The independent section tilts below were rejected: the irregularity must happen
at a real joint. They are replaced by shared post coordinates, with every rigid
rail running exactly between its two anchors. The raster rail itself was also
straightened, retaining its wood/caps/palette. Dark tint, thicker canvas and the
opaque-edge workaround remain. See `fence_joint_geometry.md` for the current
placement, source artwork and exact built-in imagegen prompt.

## Previous follow-up: darker timber, thicker sections, imperfect joints — 2026-10-04

The user accepted the general result but requested darker wood, slightly thicker
sections and small fitting imperfections, especially at right-angle corners.

- Existing wood grain/artwork is retained. The stable linear-RGB tint is now
  `0.62 + 0.025*sin(index*1.37 + side*1.91 + 0.4)` instead of `0.97 ± 0.03`.
  This darkens the timber itself, not alpha or the grass underneath it.
- Sprite canvas thickness increases from 0.55 to 0.70 board cell (+27%),
  independently of panel span; authored cell size 0.4 gives thickness 0.28.
- Each section remains rigid. Unequal post intervals still follow straight
  reference lines; there is no restored whole-side sinusoidal bow. Ordinary
  joint skews are at most 0.42°, corner sections at most 1.10°, deterministic
  and independent of frame time. The maximum end displacement is capped at
  0.055 cell even for unusually long sections.
- Non-gate sections are biased outward by their tilt reach so they do not swing
  into the sheep lane. Gate-adjacent sections pivot about the gate post, with
  the free end leaning outward; the gate anchors remain fixed. End caps overlap
  despite the small fitting offsets, and corner angles are no longer exact 90°.
- The same transforms, tints and thickness are stored for the 11 active authored
  panels in `game.world`; all IDs, inactive entries and hierarchy remain.
- `PastureFenceComposite.hpp` and the successful opaque-edge composition path
  are unchanged. PNGs, grass, worn areas, sheep and routes are not altered.

Only the working library target was rebuilt; no tests, verification builds,
offline previews or game/editor launch. User acceptance is still needed in
Play after Stop Play → Reload: wood darkness, thickness, joints/corners and exit.

## Original rigid alignment (2026-10-02)

`PastureScene.cpp` no longer offsets post anchors across the fence line. The
four sides are straight; unequal along-line post intervals, small stable tint
differences, 3-cell target spans and 0.55-cell Sprite canvas thickness remain.
The natural wood texture still supplies its small surface imperfections.
Corners and the two gate anchors are unchanged. Nine active top/left/right
Sprite transforms in `game.world` now match these straight baselines; gate-side
panels already matched. IDs, hierarchy, all 80 pooled Sprites and inactive
entries remain intact.

## Project-only workaround for Sprite alpha

The installed renderer's Sprite pass does not correctly blend the fence edge
over the underlying grass (prior diagnosis: `fence_no_outline.md`). Repainting
the PNG or padding invisible RGB did not solve that. Public Bamboo APIs also
cannot create a custom material/shader; the SDK/engine is not modified here.

Instead, on successful level/layout composition:

1. Read the existing fence texture through the authored Sprite's texture handle,
   cache its pixels, and decode RGB to linear once. Alpha remains coverage.
2. Lay out the authored fence pool and record the active panel footprints.
3. Compose the usual approved grass/wear image and decode its RGB for upload.
4. `PastureFenceComposite.hpp` source-over blends the imported wood onto the
   actual grass under each panel. It inverse-maps world positions to PNG UVs,
   filters RGB premultiplied by alpha, and averages four pixel-coverage samples.
   Only each panel's clipped bounding box is processed. The whole final image
   stays alpha 255, including the softened wood edge; no editor-panel black
   can show through that edge.
5. Assign the completed opaque background, then hide the duplicate fence
   Sprites with alpha zero. Their resources and editable scene entities remain.

The helper validates the full batch before modifying the image. All current
panels use one full-cell texture; mismatched source handles or missing/invalid
source pixels retain the ordinary Sprite fallback and log a warning. On each
layout update the active Sprite colours are set again before composition; on
shutdown the original authored colours and background texture are restored.
Only owned runtime background textures are deleted. No per-frame composition,
new material, extra runtime entity, metadata edits or new hardcoded asset ID.

## Limits and manual acceptance

- This fixes the problematic alpha path for the *gameplay* pasture. The ordinary
  edit-mode fence can still show the engine's alpha bug before scripts start.
  The underlying engine bug is not claimed fixed.
- Painted fence pixels are now on the background plane, behind sheep. Current
  escape paths keep sheep clear of the timber and posts, so no overlap is
  intended. A future mechanic that crosses a fence would need true ordered
  Sprite blending rather than this static-background workaround.
- Ground source artwork, grass sampling, wear mask/feathering, sheep animation,
  routes, gate width and HUD are unchanged. No image-generation/packing run.
- The working `liberate_the_sheep` library target was rebuilt successfully.
  No tests, test targets, verification builds, previews, editor or game ran.

After **Stop Play → Reload (Cmd/Ctrl+R)**, the user should inspect **Play**:
straight boards, corners/joins, the entire fence edge and sheep exiting the gate.
Visual acceptance of this workaround is pending.
