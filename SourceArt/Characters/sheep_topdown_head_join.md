# Standing / running head identity — 2026-09-25

Fix for the user's report that the face turns pale when running starts and the
head/neck junction looks like a transparent seam. This is a correction to the
existing offline atlas builder, not new generated artwork or a gait redesign.

## Diagnosis

Standing slot 0 and the 96 running slots already use the same texture, material,
neutral tint and no-mipmap filtering. Runtime changes only the atlas pose and
movement. The mismatch was baked in: standing used `sheep_topdown.png`, while
running kept the head of the separately painted lively source.

In the old installed atlas, centre muzzle pixel (128,30) changed from premultiplied
RGBA (123,112,109,253) standing to (154,137,130,255) running. The forehead/wool
boundary was also in a different place. The old validation compared running heads
only with their own running master, so it missed the stand/run discontinuity.
The central neck itself was opaque in inspected old frames; the visible seam
was not evidence of a full-width transparent cut or a runtime alpha bug.

## Narrow correction

- Keep each coat's existing standing pose completely unchanged.
- Extract its static hoof components using the existing verified layer helper.
  The head source contains no stationary feet to paste over the moving ones.
- Use that same coat's standing head and collar in the running body through
  row 135. Join to the old running shoulders with a smoothstep over rows 136–175.
- Interpolate premultiplied RGBA with complementary weights. Do NOT fade two
  overlapping layers with source-over: that reduces opaque coverage mid-blend.
- Composite the original moving hoof layers underneath exactly as before.
  All final pixels from row 176 downward remain byte-identical to the accepted
  running atlas. Head/neck pixels no longer come from the white RUN master.
- No changes to stride, speed, hoof paths, wool sway, tail motion, frame count,
  atlas indexing, template, .pmeta, materials, menu, fence, grass or world files.

## Regression coverage

`build_game_sheep_run.swift` now checks:

- All 288 running heads (rows 0–114) exactly equal their own standing coat.
- Central nose/neck/shoulder continuity: x112–143, y80–199 have alpha >=240.
  Natural exterior antialiasing and spaces next to ears/legs are excluded.
- Shared alpha across coats, connected silhouettes, valid premultiplication,
  protected common face colours, 96 unique poses/coat and transparent padding.
- Same existing 0.48×0.92-cell traffic envelope; actual measured half-extents
  remain 0.42041015625×0.909970703125 cells.
- A synthetic feather test keeps head/body endpoint pixels and alpha exactly.
- `--compare-head-fix BEFORE.png AFTER.png` requires all standing poses, rows
  176–511 of all 288 running cells, and empty cells to remain unchanged.

The old atlas fails the new head test (coat 0, pose 1), as intended. The new
atlas passes: 9,494,304 previously mismatched head bytes across all frames are
now equal to the standing heads. All 10 existing CTests passed (13.69 s).
Only the Swift baker was rebuilt; C++ runtime code was not changed.

## Reproduce and inspect

Use the [standard bake commands](sheep_topdown_run_prompt.md#reproducible-runtime-import).
The builder also produces `game-sheep-start-stop.png` and a start/stop section in
`game-sheep-run.html`, in addition to full-speed and quarter-speed gait previews.
The first column in the start/stop contact sheet is standing; the remaining
columns sample the running cycle for all three coats.

After saving the previous atlas, run the compiled helper:

```sh
"$sheep_preview_dir/build_game_sheep_run" --compare-head-fix \
  "$sheep_preview_dir/before.png" Assets/Textures/Characters/sheep_topdown_run.png
```

Imported atlas remains 6400×6144, 25×12, ID 3451186574. New SHA256:
`417c354159fc823b73595cec902c6d0d72681b7ef90a69f7dbc21cd4ae0afb0e`.
The old accepted-motion atlas SHA256 was
`46b570084df6bed78651c3081a27e42bdc4290d28fa6593bb9e834ec73bad795`.

Validation is offline, not an engine launch or user acceptance. Only the human
launches Panda: **Stop Play → Reload (Cmd/Ctrl+R)**, then inspect start, stop,
turns and yielding for white, black and brown sheep.
