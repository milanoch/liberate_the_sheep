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

`Scripts/liberate_the_sheep/scripts/build_game_sheep_run.swift` bakes the
[2026-09-21 grounded trot](sheep_topdown_grounded_trot.md) into
`Assets/Textures/Characters/sheep_topdown_run.png`. The existing
`sheep_topdown.png` is an input only and is never modified.

The standing pose comes from its original white sheep cell, resized to 256×512.
Standing and running share the fleece recolour for black and brown; muzzle and
pink ears retain their colours. Since the [September 25 head fix](sheep_topdown_head_join.md),
all running poses reuse the matching standing head/collar, feathered into the
running shoulders without an alpha dip. The RGB checkerboard is removed only through
edge-connected neutral pixels and its fringe is reconstructed with alpha.

The new running cycle uses the first neutral pose of the existing lively source,
not optical-flow blends between its eight differently posed sheep. The baker
extracts four isolated grey hoof components and the remaining painted body;
recombining these layers restores the original pixels exactly. It then poses
the short hooves on fixed longitudinal lanes using C++-exported ground-contact
tracks. Wool and tail receive a separate smooth weight shift, compression and
follow-through. This offline layering is flattened into complete sheep frames:
there are no extra leg entities, meshes or runtime deformation. The original
generation prompt below remains a record of how the source drawings were made,
not a description of this replacement animation pipeline.

Rebuild from the project root (the preview directory stays outside the project):

```sh
sheep_preview_dir=$(mktemp -d)
PANDA_SDK_DIR=/Applications/PandaEditor.app/Contents/Resources/PandaSDK/lib/cmake/PandaSDK \
  Scripts/liberate_the_sheep/scripts/build_scripts.sh debug
Scripts/liberate_the_sheep/bin/liberate_the_sheep_export_trot \
  "$sheep_preview_dir/trot-tracks.csv"
swiftc -O Scripts/liberate_the_sheep/scripts/build_game_sheep_run.swift \
  -o "$sheep_preview_dir/build_game_sheep_run"
"$sheep_preview_dir/build_game_sheep_run" --self-test
"$sheep_preview_dir/build_game_sheep_run" \
  SourceArt/Characters/sheep_topdown_run_8_lively_source.png \
  Assets/Textures/Characters/sheep_topdown.png \
  Assets/Textures/Characters/sheep_topdown_run.png \
  "$sheep_preview_dir/previews" \
  "$sheep_preview_dir/trot-tracks.csv"
```

Layout: 6400×6144, 25 columns × 12 rows, cell size 256×512. Each coat occupies
100 cells (four rows): standing (0), 96 running poses (1–96), empty (97–99).
Coat blocks are white, black and brown. The 25×12 dimensions are coprime for the
current Panda SDK's atlas-index workaround; runtime uses `SheepRunAnimation.hpp`.
The baker checks unchanged standing poses, lossless neutral-layer extraction,
96 distinct running poses, identical standing/run heads for every coat, opaque
neck joins, shared silhouettes
between coats, transparent padding, valid premultiplied alpha and no white
fleece islands.
It also checks the complete visible silhouette against the 0.48×0.92-cell
traffic half-extents. Frames share one scale and are registered by head/ears,
not the moving rear contour. The HTML preview plays over scrolling ground at
the actual full-speed stride duration (1.6 cells / 5.5 cells/s), alongside a
labelled quarter-speed view. The C++ tests check fixed hoof lanes, alternating
diagonals, continuous contact/loop transitions and distance-driven sampling
during acceleration, braking and stops. Mathematical contact locking is exact
for the continuous track; whole-frame atlas selection introduces up to one
frame's travel of contact-position error (1.6 / 96 cells). A GIF's minimum delay
is not used to judge this animation.

To compare against a saved previous 96-pose atlas, run the compiled baker with
`--compare-animation PREVIOUS_96.png CURRENT_96.png`. Running frames are expected
to differ: the old sideways-moving feet and rear correction are deliberately
replaced. Standing poses remain unchanged. At full speed the 96-pose cycle
lasts 1.6 / 5.5 seconds; a 60 Hz display naturally skips atlas poses rather than
slowing the cycle down. Technical validation does not replace the user's
in-editor review. Stop Play, Reload (Cmd/Ctrl+R), then inspect the movement.

For a head-only repair, use `--compare-head-fix BEFORE.png AFTER.png` instead:
it requires identical standing poses and running rows 176–511, exact head
identity with standing, continuous neck alpha and unchanged empty cells.
The preview includes a stand/run contact sheet and an alternating start/stop
canvas so a face-colour jump cannot hide in a running-only preview.

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
