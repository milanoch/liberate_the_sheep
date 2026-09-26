# Gameplay grounded trot — 2026-09-21

The user rejected the previous 96-pose cycle: hind feet spread sideways, the
gait did not match movement speed and the rear motion lacked convincing
expression. This revision changes the motion, not the frame count or art style.
Menu animation remains out of scope.

The user accepted this gameplay gait on 2026-09-22. Preserve it during unrelated
scene work; the mathematical checks below are not a substitute for that feedback.

On 2026-09-25, [standing/run head identity](sheep_topdown_head_join.md) was fixed:
the original standing head/collar is reused in all run poses with a premultiplied
shoulder join. Gait pixels below row 176 and all standing poses are unchanged.
This supersedes the original running-master head checks described below.

## Source and offline layers

Use the first neutral frame of `sheep_topdown_run_8_lively_source.png`, after
the existing checkerboard removal, fixed-scale fit and head registration. No
new imagegen request or source drawing is involved. Four short grey hoof
components are separated from the painted body. A byte-for-byte reconstruction
check requires their recombination to restore the neutral frame exactly; the
builder must fail if the source can no longer be separated safely.

The layers exist only while baking. The final imported atlas still contains
complete sheep pictures and Panda renders one Sprite per animal. There are no
runtime leg entities, bones, procedural meshes or dynamically generated
materials. The unchanged standing atlas remains an input and fallback.

## Contact-driven feet

`Scripts/liberate_the_sheep/src/Game/SheepTrotMotion.hpp` defines the motion;
`tools/ExportSheepTrot.cpp` exports the 96 samples per leg consumed by the Swift
baker. The shared stride is 1.6 cells and the rendered quad height is 1.97 cells
for a 512-pixel frame. Runtime speed remains 5.5 cells/s. Changing the stride or
quad height requires rebaking, not merely changing animation FPS.

Each hoof has a fixed lateral lane. Hind hooves therefore do not spread apart
with the wool's movement. Front-left/hind-right share one phase;
front-right/hind-left are half a cycle apart. Contact occupies 30% of a cycle.
While planted, a hoof moves backward within the frame by exactly the forward
distance travelled by the sheep. In continuous coordinates its world contact
position is stationary on straight travel; this is not turn-aware inverse
kinematics. The return stroke is a cubic Hermite curve with matching
position and velocity at toe-off, touchdown and the cycle seam. A squared-sine
lift curve eases in and out without a velocity jump; the top-down image conveys
lift through compact hoof foreshortening rather than long sideways legs.

Runtime continues to choose frames from actual travelled distance. Acceleration
and yielding therefore affect both translation and cadence; there is no separate
wall-clock animation. Whole-frame selection quantizes the ideal contact track:
the permitted contact-position error is at most one frame's travel,
1.6 / 96 cells. C++ tests check that bound at 30, 60 and 120 FPS as well as
continuous no-slip contact, fixed lanes, diagonal pairing and C1 seams. These
are mathematical/model checks, not a claim that rasterized hoof pixels are
perfectly stationary on all route turns.

A short painted shin bridges the rearward hind hoof to the body. It uses the
original dark hoof texture, sits behind the wool and does not stretch the whole
foot. Without this mostly hidden bridge, some poses leave a disconnected hoof.
The baker rejects detached alpha components of 24 pixels or more; an independent
audit also found no detached components or interior holes of at least 8 pixels
in all 96 white-coat poses (alpha threshold 32).

## Expressive wool without spreading feet

The wool shifts weight toward the supporting side while two soft
compression/rebound beats punctuate each stride. A small delayed tail motion
adds follow-through. The smooth spatial mask leaves the head fixed and grows
toward the rear; all coats receive identical motion. Hooves are composited
separately underneath and are never passed through that wool deformation.
No cyclical position offset, scale pulse or whole-Sprite wobble is added at
runtime. Sprite rotation follows only real route turns.

## Asset contract and review

The atlas remains 6400×6144: 25×12 cells of 256×512, with white, black and brown
coat blocks of 100 cells each. Local cell 0 stands, cells 1–96 run and cells
97–99 are transparent. The existing texture ID, Sprite template, coprime atlas
index workaround and no-mipmap settings are retained. Standing poses remain
unchanged; the previous running frames intentionally do not.

The baker validates distinct poses, head registration and exact standing/run
head pixels for every coat, opaque neck joins, shared alpha between coats, protected muzzle/ear colours, valid
premultiplied alpha, padding and the existing 0.48×0.92-cell traffic
half-extents. It writes a contact sheet and HTML preview with scrolling ground
at real speed and labelled quarter speed. Inspect foot contact against that
ground, not just a sheep running in place. Inspect all three colours and the
cycle seam, then have the user review acceleration, yielding and actual route
turns in PandaEditor. Only the user launches the editor/game.

The final candidate was installed at the existing asset path on 2026-09-22.
Its measured half-extents are 0.420410×0.909971 cells. Debug build and all six
CTest tests passed; `.pmeta`, texture ID and the menu were not changed.

See [the reproducible import commands](sheep_topdown_run_prompt.md#reproducible-runtime-import).
They first build/export the C++ tracks, then compile/run the Swift baker with
the tracks CSV as its fifth positional argument, after the preview directory.
`--compare-animation PREVIOUS_96.png CURRENT_96.png` compares the old and new
atlas without requiring the rejected motion to remain pixel-identical. After
installing a validated atlas and rebuilding scripts, stop Play and Reload
(Cmd/Ctrl+R) before the user's visual review.
