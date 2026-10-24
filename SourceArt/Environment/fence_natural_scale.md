# Natural fence proportions and alignment — 2026-10-02

Follow-up: the user rejected the whole-boundary bow. It is now removed, while
unequal post spacing and this wood artwork remain. Gameplay also uses an opaque
fence/ground composite to bypass broken Sprite alpha; see `fence_opaque_composite.md`.
The text below records the original natural-fence iteration.

The user found the perfectly straight fence artificial and poorly proportioned
against the sheep. This revision keeps the approved illustrated grass and worn
areas, and changes only the wooden fence artwork and authored/runtime placement.
It is a top-down illustrated pasture with more plausible proportions, not a
camera change or a conversion to photorealistic/3D gameplay.

## Artwork

Built-in `imagegen` edited `wood_fence_oak_no_outline_source.png` into matte,
lightly weathered oak: restrained uneven edges, a slight bow between fixed
post ends, wood grain, knots, shallow checks and differently worn post caps.
A second targeted edit requested cleanup of colored edge flecks. The production
packer retains alpha and extends wood RGB into invisible edge pixels, as before.

- Final source: `SourceArt/Environment/wood_fence_natural_source.png`.
- Final generation output:
  `/Users/milan/.codex/generated_images/01a03de4-d2db-7173-a871-84b0596ed643/exec-8dc867f5-4b7c-4ea5-a407-09944804da64.png`.
- First edit output:
  `/Users/milan/.codex/generated_images/01a03de4-d2db-7173-a871-84b0596ed643/exec-843cdf61-d320-4816-854b-e238d635231a.png`.
- Imported resource: `Assets/Textures/Environment/wood_fence_panel.png`.
- Same 1985×283 canvas and editor-issued ID **1435494927**; `.pmeta` untouched.
- Prior source artwork retained; no new material, mesh or runtime scene object.

Applied with the existing asset packer:

```sh
swift SourceArt/Environment/prepare_pasture_assets.swift \
  SourceArt/Environment/wood_fence_natural_source.png \
  Assets/Textures/Environment/wood_fence_panel.png \
  1985 283 --bleed-transparent-rgb
```

## Scale and shared posts

`PastureScene.cpp` repositions the existing authored Sprite pools:

- Target section span is 3 board cells, chosen with nearest-count rounding,
  compared with the sheep Sprite's 1.97-cell length. This is a visual relative
  scale, not a claim of measured metres.
- Canvas thickness is fixed at 0.55 cell rather than tied to section length.
  Visible timber occupies only part of that transparent canvas. Short gate-side
  sections no longer become thinner merely because their span is shorter.
- Adjacent sections use exactly the same deterministic anchor for each joint.
  Interior anchors vary along the fence by at most 8% of one interval and bow
  outward by at most 0.14 cell; offsets never bend toward the sheep corridor.
- Corners and gate anchors stay fixed. Sections rotate toward their shared
  endpoints, with length divided by the existing 0.94 post-center span.
- Stable RGB tint variation stays between 0.94 and 1. No animated jitter,
  independently rotated/disconnected sections, random scene objects or changes
  to sheep movement, pathfinding, collision rules or logical gate width.

`game.world` contains the matching authored appearance for its current
0.4-world-unit cell size: canvas thickness 0.22, active panel counts 3/3/3/1/1.
All 16-per-side pools, IDs and parent/child links remain. Newly unused panels are
hidden with alpha zero; previously inactive transforms are retained.

## Applying and manual review

Only the working script target was built successfully:

```sh
cmake --build Scripts/liberate_the_sheep/cmake-build-debug \
  --target liberate_the_sheep --parallel 4
```

No tests, verification builds, offline previews, editor or game were run.
The user applies **Stop Play → Reload (Cmd/Ctrl+R)** and judges fence proportions,
natural alignment, joints/corners and the exit while sheep are running.

The previously diagnosed dark alpha edge in the Panda Sprite pass is a separate
unresolved engine issue (`fence_no_outline.md`). This artwork/placement revision
does not claim to fix blending and does not modify the SDK or engine.

## Exact prompts — built-in imagegen

### Natural oak edit

```text
Use case: precise-object-edit.
Asset type: transparent 2D top-down wooden fence section sprite for a sheep pasture game.
Input image 1 is the edit target: a very perfectly straight, glossy-looking wooden rail between two square post caps.
Primary request: make THIS rail and its post caps look naturally built from solid, lightly weathered farm oak, keeping the polished hand-painted game style. Replace the factory-straight uniform edges with a very slight organic bow and small variations in timber thickness. Add restrained irregular wood grain, two or three tiny natural knots, a few shallow hairline checks and softened worn corners. The two post caps should have subtly different growth rings, small non-identical corner wear and a natural sawn-wood surface. It is a well-kept sturdy working pasture fence, not broken, rotten, cheap sticks or a fantasy barricade.
Palette: matte warm muted honey/tan oak, slightly desaturated, complementing green cartoon grass and cream, brown and charcoal sheep. NO orange varnish, yellow shine, white highlights, dark ink outline or black rim. Model wood volume with small gentle tonal changes within the silhouette, not exterior shadows.
Composition invariants: strict vertical 90-degree bird's-eye orthographic view; ONE horizontal fence section; one connected wooden top rail and exactly two end-post top faces. Keep both post centers on the same horizontal axis, at about 3.5% and 96.5% of the full artwork width. Keep the width and post size close to the reference. Bow the rail very slightly BETWEEN the posts, never move the attachment points. Both ends connect firmly to their posts. Post caps remain approximately square, not oversized blobs.
Do not show tall upright post sides, front elevation, perspective, two parallel rails, cross-braces, screws as large black dots, rope, wire, extra posts, grass, soil, animals, cast shadows, scenery, a border, text or watermark. No detached chips or speckles outside the wood.
Background/output: genuinely transparent RGBA PNG, retaining clean crisp anti-aliased wood silhouette and transparent empty margins. No opaque background or checkerboard painted into the image. The source is packed into a 7:1 canvas later; do not stretch the wood into a thick wall.
```

### Targeted edge cleanup

```text
Use case: precise-object-edit.
Input image is the edit target: a horizontal hand-painted oak fence section with a post at each end on a transparent background.
Change ONLY the alpha silhouette and its immediate edge pixels: completely remove the tiny saturated RED and YELLOW stray flecks/fringes visible just outside the wooden rail and post silhouettes. All empty space outside the actual wood must be fully transparent; any anti-aliased boundary pixels must use natural tan/brown wood colors, never red, yellow, black or white halos. Keep edges crisp and clean, with no detached speckles.
Preserve the natural oak grain, tiny cracks, slight rail bow, knots, matte brown/tan palette, exact position, dimensions, post centers and all wood interior pixels/appearance. Do not smooth away texture or redesign anything. Keep one connected rail and exactly two square post tops, strict bird's-eye orthographic view. Do not add a background, cast shadow, outline, new objects or text.
Return transparent RGBA PNG with the same composition.
```
