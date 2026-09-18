# More legible gameplay trot — 2026-09-15

Built-in `imagegen` edit of `sheep_topdown_run_8_refined_source.png`.
Selected output: `sheep_topdown_run_8_lively_source.png`.
The accepted earlier source is retained. No menu artwork is involved.

These are eight source key poses. The current offline import, revised again on
2026-09-18 after the user's feedback on the rear motion, adds eleven
motion-compensated in-betweens per interval, including the loop closure, for
96 run poses. Runtime uses a 25×12 atlas (6400×6144), 100 cells per coat:
standing 0, running 1…96, empty 97…99. It replaces the earlier 48-pose
density-only update. Source drawings, standing poses, speed and stride are
unchanged; running-frame interpolation and rear motion are revised below.
No new imagegen art is used in this revision. All three coats are derived
consistently; runtime never rotates the entire Sprite to simulate pelvic motion.
Speed and stride remain 5.5 and 1.6 cells.

## Historical offline motion refinement — 2026-09-16 (superseded)

The initial generated sheet alone did not increase measured rear excursion:
17.33 px versus 19.62 px in the accepted atlas. A further built-in imagegen edit
produced edge artifacts and was rejected, not installed.

That revision applied a small deterministic rear-motion gain in the
animation baker. Displacement followed each key's rear-third alpha
centroid about the cycle's midpoint. Extra gain is 0.65, eased vertically from
62.5% to 82% of cell height. The front half remains byte-identical, no whole-Sprite
rotation or scale is introduced, and no separate limbs are rigged. This gives
25.41 px rear excursion (about 30% above the accepted atlas) while preserving
the tested 0.48×0.92-cell half-extents. Originally two optical-flow in-betweens
were added for every pair of processed keys; the earlier 2026-09-18 revision
increased this to five, including the cycle seam. These measurements describe
that old implementation, not the current 96-pose revision.

## Current continuous rear motion — 2026-09-18

The old per-key displacement gain is removed. The baker first interpolates the
original aligned keys into 96 poses, then fits the phase of a first-harmonic
cycle to their eight rear-third alpha centres. Each dense pose receives a
rear-only translation so its rear centre follows a continuous sinusoid with an
amplitude of up to 14 pixels, constrained by the existing collision envelope.
The baker selects one amplitude and centre for the entire cycle from the safe
translation ranges of all 96 poses. It tests 14 down to 10 pixels in 0.25-pixel
steps and keeps the centre within 6 pixels of the painted mean; individual
frames are never clipped to flatten the curve's peaks. The installed cycle
uses 11-pixel amplitude, about 22 pixels peak-to-peak. The correction eases in vertically
from 62.5% to 82% of cell height; the front half is byte-identical before and
after correction. There is no whole-body rotation, scaling, extra runtime sway
or separate-limb rig. The baker checks the corrected centre against its target
and keeps the existing 0.48×0.92-cell traffic half-extents.

Motion-aligned RGB samples now blend with smooth continuous weights instead
of changing source abruptly at the midpoint of each key interval. Soft-alpha
endpoint handling also keeps painted-key edges consistent with their adjacent
in-betweens. Standing poses and painted-key front halves remain exact; the
entire running frames intentionally change and are not required to match the
older atlas. The importer reports cyclic contour and rear-motion checks; final
visual approval remains a human in-editor check, not a claim based on frame
count alone.

Coats are derived BEFORE displacement/interpolation so contour changes cannot
change the fleece mask. All coats share flow and alpha; muzzle and pink-ear
pixels retain the white master's identically warped RGB to prevent bilinear
sampling from pulling coat colour across an ear/fleece boundary.

## Exact edit prompt

Use case: identity-preserve. Asset type: top-down sheep running animation keyframes for an existing game.
Image 1 is the EDIT TARGET, an eight-pose white sheep sheet already approved in style. Make just one improvement: the local pelvic/short hind-hoof weight transfer should be around 25% more legible than in this input, not a redesign.

Keep the exact EIGHT cells in FOUR columns and TWO rows on a square canvas, with the same animal size and nose-up orthographic overhead viewpoint. Keep head, ears, shoulders, front half, wool identity, long narrow proportions, lighting and palette unchanged. No whole-body yaw, rotation, translation, stretching or wobbling. All heads stay fixed and straight UP.

Only adjust the rear THIRD: a small but clearly readable left/right transfer through the hips synchronized to the short hind hooves and tiny tail. Retain the existing loop order: centered frame1; leftward frames2,3,4 with peak frame3; centered frame5; rightward frames6,7,8 with peak frame7; frame8 returns smoothly toward frame1. Increase this pelvic motion by about a quarter, NOT several times. The pelvis is a coherent soft volume and a natural continuation of the still torso, not a hinged segment. Short legs move fore/aft under wool rather than stretching out sideways. Add slightly clearer hoof contact/passing poses, still mostly covered by wool from above. Gentle funny brisk sheep trot, not dancing, spider limbs, sliding feet or jelly.

Keep all eight animals compact and fully inside each cell with generous margins; don't broaden the animals or distort anatomy. Preserve the front-half registration across every frame. Square canvas, same strict4x2 grid, no extra frames, no labels/text/grid lines/ground/shadows. Genuinely transparent RGBA background; do not paint a checkerboard. This remains a small, recognizable update of the provided eight poses; intermediate frames will be added offline.
