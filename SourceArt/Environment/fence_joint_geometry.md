# Straight rails, shared post joints — 2026-10-04

The user clarified that fence imperfections belong at the connections, not in
the middle of the timber. This supersedes the independent per-panel rotations
in `fence_opaque_composite.md`.

## Artwork

Built-in `imagegen` edited only the long rail of
`wood_fence_natural_source.png`: a straight horizontal axis and parallel long
edges, retaining the wood grain, end caps and palette. Original source retained.

- Generated original:
  `/Users/milan/.codex/generated_images/01a03de4-d2db-7173-a871-84b0596ed643/exec-72503d11-c182-4608-9375-c9148343f4b4.png`
- Project source: `SourceArt/Environment/wood_fence_straight_source.png`.
- Runtime texture: `Assets/Textures/Environment/wood_fence_panel.png`.
- Existing 1985×283 canvas, texture ID 1435494927 and metadata retained.
- Packaging uses the existing aspect-fit crop/RGB-bleed script; alpha is retained.

```sh
swift SourceArt/Environment/prepare_pasture_assets.swift \
  SourceArt/Environment/wood_fence_straight_source.png \
  Assets/Textures/Environment/wood_fence_panel.png \
  1985 283 --bleed-transparent-rgb
```

### Exact built-in edit prompt

```text
Use case: precise-object-edit
Asset type: existing transparent top-down wooden fence panel sprite for a 2D sheep game.
Input image 1: edit target, preserve its identity and artwork.
Primary request: straighten ONLY the long horizontal wooden rail. Its centerline and both long silhouette edges must be perfectly straight, parallel horizontal lines from left post cap to right post cap. Remove the slight sag/bow and all broad undulation of the rail. It must read as one rigid sawn timber board, not a curved branch. The game will place different straight panels at slight angles where they meet at posts.
Preserve: the same two end post caps, cap centers approximately 3% and 97% of the full visible object width, same relative cap and rail widths, same wood-grain detail, knots, warm matte wood palette, top-down orthographic view, painted game art style. Keep both caps centered on exactly the same horizontal axis and the board joined to each cap. Preserve the original long thin silhouette proportions.
Background: genuine fully transparent RGBA, isolated sprite, all parts fully inside image with small transparent margins.
Do not add shadows, black outlines, new objects, grass, text or gloss. Do not curve the silhouette, add perspective, or recolor the wood. Texture grain can remain organic, but the outside long edges and axis must be straight.
```

## Current placement: mixed directions and bottom joints — 2026-10-06

The stronger side geometry was approved. The user requests similar bottom
imperfections and a mix of inward/outward bends, with the current strength.

- Top and right stay unchanged. The left normal offset changes from -.12
  to +.12 cell, retaining its 3.24-degree base budget and hinge location.
- Bottom-left gets an outward hinge at 58%, bottom-right an inward hinge at
  43%. Signed offset caps are +.10/-.10 cell; budgets are 3.1/2.9 degrees.
- All five profiles now use signed `copysign(min(abs(offset)*cell,
  budgetRadians*a*b/L),offset)` heights. Exact-distance interpolation and
  shared post anchors are unchanged.
- Each bottom half uses at least two existing pooled sections, so a small
  pen also has a real joint without bending its raster artwork. Authored
  active counts are 3/3/3/2/2; the additional two Sprites were already in the
  scene. Their tint follows the same per-side/index rule as all active panels.
- The two gate anchors and shared corners stay fixed, preserving the opening
  and escape paths. Panel height, artwork, alpha workaround, camera and ground
  are not changed. This is a change in direction/distribution, not a return
  to repeated zigzags or stronger bends throughout the fence.

Visual acceptance remains with the user after Stop Play → Reload → Play.

## Previous accepted placement: balanced, unequal imperfections — 2026-10-06

The user found the two-kink version too straight. This is an intermediate
layout, retaining straight timber and shared post connections:

Follow-up: the user approved this arrangement and requested slightly more
curvature on the left/right only. Side offsets and their angle budgets are
now 20% stronger; all hinge locations, corners and top/bottom runs are retained.

- One top hinge at 60%, one lighter left hinge at 38% from the bottom.
- Two unequal right hinges at 24% and 73% from the bottom.
- Hinge indices are rounded to existing posts and clamped to internal posts.
  The second right hinge is only used with at least three panels. There is
  no repeating pattern or increasing kink count on larger boards.
- First hinges use negative normal offsets bounded by .16/.12/.12 cell
  (top/left/right). Heights also use the previous `budget*a*b/L` bound with
  base budgets 4.0/3.24/3.6 degrees. The second right offset is +.030 cell,
  introducing a weaker opposite-direction correction; it changes the actual
  first hinge angle too, so the base budget is not a total-angle guarantee.
- All intermediate posts are interpolated in actual along-distance, keeping
  intervening joins collinear despite unequal board lengths.

Shared corners have small, unequal offsets in cell units:
TL(+.035,-.015), TR(-.045,+.020), BL(-.025,+.055), BR(+.015,-.035).
This breaks the exact rectangle without splitting corner posts. The two gate
anchors remain pinned. Bottom runs are straight between corner and gate.

World transforms match the same scheme. Art, tint, thickness, opaque-edge
composition, camera, grass and sheep paths are unchanged. Visual realism is
pending user acceptance in Play after Reload; no automated tests or previews.

## Previous placement: sparse, restrained imperfections — 2026-10-05

The joint-angle concept was accepted, but the user found the kinks too numerous,
too strong and poorly distributed. Replace the repeating offset pattern with
exactly two separated hinge posts (when those sides have at least two panels):

- Top: `joint = clamp(round(count*.68),1,count-1)`, pointing inward.
- Right: `joint = clamp(round(count*.28),1,count-1)`, pointing outward.
- Left and the two gate-side runs stay completely straight.

Using the existing unequal post spacing, let `a` be the hinge's along-distance,
`b=L-a`. Top height is `min(.12*cell, radians(2.3)*a*b/L)`; right height is
`min(.04*cell, radians(1.2)*a*b/L)`. Actual angle is `atan(h/a)+atan(h/b)`,
so the budgets are upper bounds and longer sides can be gentler. The normal
offset interpolates linearly in actual distance from zero at the corner to
the signed hinge height and back to zero at the other corner. Consequently
there are no accidental bends at the intermediate posts, no repeated zigzag
and no extra kinks as the board grows.

Corner/gate positions, sheep paths, camera, wood art, tint, thickness and
opaque edge composition are unchanged. Corner angles vary mildly through
incident rail directions, without independent panel rotations or gaps.
`game.world` stores the same placement for its active panels.

## Previous placement: visible joint angles (too strong/frequent)

The user's follow-up clarifies the intended shape: roughly 170/190-degree
connections mixed with straight 180-degree connections, and corners slightly
either side of 90 degrees. The previous tiny outward offsets were insufficient.

The nominal four corner coordinates are now fixed and shared by both sides.
Interior anchors use signed normal offsets, in cell units:
`{-0.22,+0.04,-0.22,-0.22,-0.22,+0.04,+0.04,+0.04}`.
Per-side phases are `{0,4,1,2,5}`; index is `(post-1+phase[side]) % 8`.
Opposite offsets differ by .26 cell across an approximately 3-cell span,
giving headings near +/-5 degrees and turns up to about 10 degrees. Exact
angles vary with the preserved unequal post intervals. Repeated offset levels
leave collinear joints. The first internal post of the left side is explicitly
interpolated between its corner and second post, by actual along-distance,
so even a small three-panel side includes a straight connection.

Offsets are measured from the nominal side, not from displaced corners.
Their asymmetric bounds limit intrusion toward the running lane and keep the
outward excursion within the existing camera margin. The two gate posts stay
pinned; escape paths, layout/camera, panel count, tint, thickness and images
do not change. Corner angles change because their incident panels change
heading, not because the post is split or the boards no longer meet.

Each panel still uses midpoint `(A+B)/2`, width `length(B-A)/.94` and rotation
`atan2(B.y-A.y,B.x-A.x)`. The active authored world transforms use this same
placement. No individual rotation is applied after joining shared anchors.

## Previous placement (superseded by the follow-up above)

Each rigid panel is derived only from two shared post anchors: midpoint,
length and direction. Adjacent panels use the same joint; there is no extra
per-panel skew or positional correction after this calculation. Small,
nonperiodic outward post offsets and shared offset corners create angular
changes at the posts. The two gate anchors remain fixed.

Corner offsets in cell units are TL (-.07,+.12), TR (+.11,+.03),
BL (-.04,-.10), BR (+.08,-.02). Interior post offsets use
`{.015,.075,.025,.060,.035,.095,.010,.050,.085,.020,.065,.040,.100,.030,.055,.080}`
with index `(postIndex + side*5) % 16`, outward from the corner baseline.
The former unequal along-line spacing is retained. Panel center is `(A+B)/2`,
width is `length(B-A)/.94`, and rotation is `atan2(B.y-A.y,B.x-A.x)`.

The current darker tint (0.62±0.025), canvas thickness (0.70 cell), target span
(3 cells), opaque edge compositor, grass/wear and sheep routes are retained.
The active authored panels in `game.world` mirror the runtime placement.

Only the working `liberate_the_sheep` library target was rebuilt successfully
for Reload. No tests, test builds, previews, editor or game were run.
Visual acceptance is performed by the
user in Play after Stop Play → Reload (Cmd/Ctrl+R): straight board spans,
connected posts/corners, unchanged gate clearance and no black edge regression.
