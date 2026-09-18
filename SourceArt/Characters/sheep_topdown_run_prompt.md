# Gameplay top-down running sheep

Generated with the built-in `imagegen` tool on 2026-09-12, using
`sheep_topdown_white_source.png` as the identity reference. Not a menu asset.
Original output: `sheep_topdown_run_8_source.png` (retained).
Current selected source, after the user's 2026-09-15 request for a clearer,
smoother, faster gait: `sheep_topdown_run_8_lively_source.png`.
See [the current edit prompt](sheep_topdown_run_lively_prompt.md).
The accepted 2026-09-14 source and its [prompt](sheep_topdown_run_refined_prompt.md)
remain available for comparison/recovery.

The tool returned an RGB sheet with a painted checkerboard despite the request
for alpha. A follow-up background-extraction request did the same and was not
selected. The offline atlas baker removes only edge-connected neutral
background before fitting the painted whole-animal frames and deriving coats.

## Reproducible runtime import

`Scripts/liberate_the_sheep/scripts/build_game_sheep_run.swift` packs the selected
whole-animal artwork into `Assets/Textures/Characters/sheep_topdown_run.png`.
The existing `sheep_topdown.png` is an input only and is never modified.

The standing pose comes from its original white sheep cell, resized to 256×512.
Both standing and running then use the SAME fleece recolour for black and brown;
ears remain the same pink across all poses. Importing the old coloured standing
cells directly would cause different highlights/ear colours when the run starts.
The generated RGB checkerboard is removed only through edge-connected neutral
pixels; its antialias fringe is reconstructed with transparent alpha. No limb
rigging or procedural replacement of painted animals is used. Bidirectional
macOS Vision optical flow adds eleven intermediate poses between each pair of
painted keys, including key 8 → 1. A signed-distance matte interpolates outlines,
with soft-alpha endpoint handling; motion-aligned colour samples blend
continuously instead of abruptly switching source at the interval midpoint.
This is not a dissolve between two unaligned whole-animal images.

After dense interpolation, a rear-only horizontal correction normalizes the
gait's uneven lateral motion onto one periodic curve. Phase is fitted to the
eight painted rear centres. Amplitude is up to 14 pixels, constrained by the
existing collision envelope: the baker selects one safe amplitude and centre
for the whole cycle instead of clipping individual poses. It tries amplitudes
from 14 down to 10 pixels in 0.25-pixel steps, allowing the centre to move at
most 6 pixels from the painted mean, and intersects all 96 poses' safe ranges.
The installed cycle uses 11-pixel amplitude (about 22 pixels peak-to-peak).
The correction eases in from 62.5% to 82% of cell height and leaves the front
half byte-identical. It replaces the earlier per-key gain, with no whole-Sprite
wiggle or runtime deformation. The eight keys are recoloured before any
interpolation or correction. All three coats share the white keys' optical flow,
silhouette and rear correction; muzzle and pink-ear colours stay protected.
Re-running a fleece flood fill on warped frames could accidentally recolour
the face.

Rebuild from the project root (the preview directory stays outside the project):

```sh
sheep_preview_dir=$(mktemp -d)
swiftc -O Scripts/liberate_the_sheep/scripts/build_game_sheep_run.swift \
  -o "$sheep_preview_dir/build_game_sheep_run"
"$sheep_preview_dir/build_game_sheep_run" --self-test
"$sheep_preview_dir/build_game_sheep_run" \
  SourceArt/Characters/sheep_topdown_run_8_lively_source.png \
  Assets/Textures/Characters/sheep_topdown.png \
  Assets/Textures/Characters/sheep_topdown_run.png \
  "$sheep_preview_dir/previews"
```

Layout: 6400×6144, 25 columns × 12 rows, cell size 256×512. Each coat occupies
100 cells (four rows): standing (0), 96 running poses (1–96), empty (97–99).
Coat blocks are white, black and brown. The 25×12 dimensions are coprime for the
current Panda SDK's atlas-index workaround; runtime uses `SheepRunAnimation.hpp`.
The baker checks identical standing/run coat processing, unchanged white
standing geometry, 96 distinct poses, eight preserved key front halves, shared silhouettes between coats,
transparent padding, valid premultiplied alpha and no white fleece islands.
It also checks the complete visible silhouette against the 0.48×0.92-cell
traffic half-extents. Frames share one scale and are registered by head/ears,
not the moving rear contour. The HTML preview plays at the actual full-speed
stride duration (1.6 cells / 5.5 cells/s), alongside a labelled quarter-speed
version for inspecting intermediate poses. A GIF's minimum delay is not used
to judge this animation. Cyclic contour-jump metrics include the seam, and the
rear-motion checks cover the periodic curve and fixed front half.

## Current animation refinement — 2026-09-18

This 96-pose revision supersedes the earlier 48-pose density-only update on the
same date. The source artwork, standing poses, speed (5.5) and stride (1.6) are
unchanged; running-frame colour interpolation and rear motion are deliberately
reworked to remove discontinuities. It is no longer correct to require all old
running frames to remain pixel-identical. Only the painted keys' front halves
and the unchanged standing poses retain that guarantee. No new imagegen
request was needed for this revision.

To compare against a saved previous 48-pose atlas, run the compiled baker with
`--compare-animation PREVIOUS_48.png CURRENT_96.png`. This comparison is for
animation continuity and preservation checks, not the superseded requirement
to retain every previous whole-body run frame. At full speed the 96-pose cycle
lasts 1.6 / 5.5 seconds: runtime samples it by distance, so a 60 Hz display
naturally skips several atlas poses rather than slowing the cycle down.

## Generation prompt

Use case: identity-preserve.
Asset type: transparent 2D game sprite sheet, exactly EIGHT whole-animal frames,
four columns and two rows.
Input image 1: reference and identity to preserve; existing top-down white sheep
game sprite. Keep the SAME sheep, cream-white long woolly oval body, small dark
grey head, pink inner ears, tiny dark short hooves, tiny fluffy tail, detailed
soft painted wool.

Primary request: derive one gentle funny running/trotting cycle from this
original top-down sheep. EXACTLY orthographic overhead view, nose pointing UP
in all eight cells, never side view or oblique. Keep the original tall narrow
body silhouette. Each cell has the complete sheep from nose to tail with
generous transparent padding, identical body size, centre, framing and lighting.

Composition: square sprite-sheet canvas with precisely FOUR equal columns and
TWO equal rows. Eight separate evenly-spaced tall narrow sheep, one centred in
each cell, strict regular grid with no visible grid lines. Frames read left to
right then next row. Actual transparent background, no checkerboard pixels, no
ground, no text, no labels.

Animation: a smooth compact 8-pose trot loop. Frame 1 neutral contact; frame 2
left-front/right-rear tiny hooves move rearward as other diagonal lifts; frame 3
soft buoyant body lift and ears lag; frame 4 return to passing; frame 5 opposite
diagonal contact; frame 6 right-front/left-rear move rearward; frame 7 second
soft lift; frame 8 passing back toward frame 1. Show only VERY SHORT natural
sheep hooves poking just outside the wool, primarily near the narrow front and
rear corners. Legs are mostly concealed by the wool from this overhead view.
Body stays the same animal throughout; subtle squash, wool wobble, head nod and
ear bounce no more than 3-4 percent, not independent jelly shapes.

Invariants: same wool marking structure and face across every frame, same body
proportions, no new costume, no horns, no eye/face rearrangement, all heads point
UP. No long lateral spider legs, no detached parts, no huge knees, no extra
limbs, no shadow, no pedestal, no perspective shift. This is eight sequential
whole-animal sprite poses suitable for a looping animation, not eight different
sheep designs.

## Follow-up alpha request (not selected)

Use case: background-extraction. Edit target: attached eight-frame 4-column
2-row sheep sprite sheet. REMOVE ONLY the grey/white checkerboard background,
making it genuinely transparent with a real alpha channel. Preserve all eight
sheep pixels, short hooves, exact poses, wool, head, ears, size, centering and the
strict 4x2 layout. The checkerboard is a mistaken painted background; erase it
entirely to transparent. No checkerboard, no solid white or black background,
no shadows. Do not redraw or re-pose the sheep. Output actual RGBA PNG with
transparent background.
