# Source art

This directory contains editable/generator source images. PandaEditor does not
load these files at runtime; the imported, optimized atlases live under
`Assets/Textures/Characters/` and retain their editor-issued `.pmeta` IDs.

## Gameplay pasture and top-down run — 2026-09-12

The current level art is `Assets/Textures/Backgrounds/pasture_grass.png`,
`Assets/Textures/Environment/wood_fence_panel.png`, and
`Assets/Textures/Characters/sheep_topdown_run.png`. All are imported resources,
created with the built-in imagegen tool from saved requests:
[pasture prompts](Environment/pasture_assets_prompts.md) and
[sheep prompt/import instructions](Characters/sheep_topdown_run_prompt.md).
The original standing atlas is preserved as an input and fallback.

The current sheep atlas is 6400×6144, twenty-five columns by twelve rows. Each
coat owns four rows / 100 cells: local cell 0 stands, 1–96 run, and 97–99 are
transparent padding. Coat blocks
are white, black and brown. Standing and running share
the same coat conversion, including ear color. Runtime uses the SDK-compatible
atlas index mapping and no mipmaps. The 1.23×1.97-cell quad includes transparent
padding. The refined run fits a 0.96×1.84-cell traffic envelope, including ears,
hooves and pelvic movement.

On 2026-09-14 the user rejected whole-Sprite pivot rotation and accepted the
in-frame gait direction. On 2026-09-15 they requested more legible pelvic/hoof
movement, more frames and faster running. The current eight key poses come from
`Characters/sheep_topdown_run_8_lively_source.png`, a small built-in imagegen edit
of the accepted source. The [exact edit prompt](Characters/sheep_topdown_run_lively_prompt.md)
and both older sources are retained.

The latest 2026-09-18 revision replaces the earlier 48-pose density-only update.
It uses the same eight source drawings, with eleven motion-compensated
in-betweens per interval, including key 8 → 1, for 96 poses. No new imagegen art
is used. Motion-aligned colour samples now blend continuously instead of
switching source at each interval's midpoint; soft-alpha endpoint handling
avoids alternating between different edge treatments at the painted keys.
After interpolation, the rear follows one periodic side-to-side curve: its
phase comes from the source gait, with an amplitude of up to 14 pixels
constrained by the existing traffic envelope. The baker fits one safe amplitude
and centre for the entire cycle, not per-frame clipping. A rear-only horizontal
correction eases in from 62.5% to 82% of cell height;
the front half is unchanged by this correction. This replaces the old per-key
displacement gain rather than stacking another sway on top of it. Standing
poses remain exact, as do the painted keys' front halves; entire running
frames are intentionally no longer pixel-identical to the 48-pose atlas.
All coats use the same white-coat optical flow, silhouette and rear correction;
there is no independent timing or unaligned whole-animal alpha dissolve.
The importer aligns frames by head/ears, not the moving rear silhouette; runtime
does not add whole-body rotation, position sway or scale pulses. Only actual
route turns rotate the Sprite. Gait phase still follows distance and slows with
yielding. Speed remains 5.5 cells/s and stride remains 1.6 cells: extra frames
do not slow the gait, and the renderer selects the pose for actual distance
rather than trying to display every atlas cell. The importer emits an HTML
preview at actual cadence and a labelled quarter-speed view, plus cyclic
contour progression and rear-motion metrics. Its checks retain the existing
traffic envelope and shared silhouettes between coats; visual acceptance still
requires the user's in-editor review.
Menu artwork and motion are untouched by this correction.

The lawn covers the viewport without changing aspect ratio; fence sections
retain their approximately 7:1 proportions and overlap their end posts by 6%.
Fence Sprites, background and a hidden animation-template Sprite are authored
in `game.world`; scripts position them but do not generate textures/materials.
Only the procedural flock is instantiated at runtime.

Menu animation is still not visually accepted and was explicitly put on hold
by the user. Do not resume the menu rig/interpolation work as part of gameplay
changes. The technical description below records its existing state only.

## Current side-view sheep animation

The menu sheep uses the original whole-animal drawings from
`Characters/sheep_side_run_16_source.png` and
`Characters/sheep_side_jump_24_source.png`. The offline workflow adds two
motion-compensated in-betweens between each consecutive pair of original run
poses, including the loop boundary. It retains all 16 original poses at every
third output frame and keeps the 24 original jump poses. The previous ordering
of both source sequences is baked into the atlas, so runtime playback is linear.

The interpolation step uses macOS Vision optical flow offline, not a runtime
animation rig. All coats share the same motion field. A signed-distance matte
interpolates the silhouette instead of alpha-dissolving two overlapping legs;
texture samples come from a single source at each point rather than a blurred
mixture. Inspect the generated contact sheet for doubled legs, holes, changing
coat color and a discontinuous loop; frame count alone does not establish
animation quality.

Rebuild in two stages from the repository root. Keep the old-format intermediate
and the unused top-down output in a temporary directory; do not overwrite the
gameplay atlas or install the old `16×9` menu atlas directly:

```sh
sheep_restore_dir=$(mktemp -d)
swift Scripts/liberate_the_sheep/scripts/build_sheep_animation.swift \
  SourceArt/Characters/sheep_side_run_16_source.png \
  SourceArt/Characters/sheep_side_jump_24_source.png \
  SourceArt/Characters/sheep_topdown_white_source.png \
  "$sheep_restore_dir/sheep_side_original.png" \
  "$sheep_restore_dir/sheep_topdown_unused.png"
swift Scripts/liberate_the_sheep/scripts/build_sheep_inbetweens.swift \
  "$sheep_restore_dir/sheep_side_original.png" \
  Assets/Textures/Characters/sheep_side_run.png \
  "$sheep_restore_dir/previews"
```

The final argument is a required preview directory. Appending `--preview` runs
only a short interpolation sample and does not write the runtime output atlas.
The original-source builder removes the generated checkerboard backgrounds,
aligns poses by the wool-torso anchor and derives black/brown coats from white
masters. The second builder densifies the animation without replacing the sheep
with separately posed body parts. Panda still renders one Sprite per sheep and
creates no runtime bones, meshes or textures.

The transparent atlas is `3072×2880`: `16×15` cells of `192×192`. Each coat occupies
80 cells (five rows): run frames `0…47`, jump frames `48…71`, then eight reserved
cells. Coat starts are `0`, `80`, `160`. The existing editor-issued texture ID
`2584797671` is retained; mipmaps stay disabled to prevent cell/coat bleeding.
The coprime grid dimensions also support the current PandaSDK atlas-index
workaround in `MenuDiorama`.

The run cycle is distance-driven: one full cycle covers `1.75` world units at
`1.75` world units/s (`1 s`). Extra frames do not increase the gait's cadence.
Runtime adds the jump's ballistic rise, not a second layer of body bobbing or
deformation. The normal jump is `0.90` world units high and travels at `1.925`
world units/s, only 10% faster than running. The current hay geometry gives about
`1.39 s` airborne, with a separate `0.16 s` speed transition on either side.
Jump width comes from silhouette clearance rather than a fixed short duration;
the hay's transparent side margins are not treated as a solid obstacle.

The gameplay atlas `Assets/Textures/Characters/sheep_topdown.png` is unchanged by
this menu-animation workflow.

## Rejected articulated-rig experiment

On 2026-09-11 the user rejected the animation made from
`Characters/sheep_side_rig_parts.png`: its legs looked spider-like and its
`0.74 s` flight moved too abruptly. The parts, the saved imagegen request
`Characters/sheep_side_rig_prompt.md`, and `build_sheep_rig.swift` remain only as
an archived rejected experiment. Do not use that builder to regenerate the
current runtime atlas or silently replace the whole-animal art with it again.

That version passed technical checks and still failed visual review. A clean
build, valid atlas and more frames are not substitutes for the user's in-editor
animation check. Only the user launches PandaEditor/the game; after changes,
stop Play, Reload (Cmd/Ctrl+R), then review the restored run and jump in motion.

## Legacy menu ground

Rebuild the tightly cropped menu ground with:

```sh
Scripts/liberate_the_sheep/scripts/build_menu_ground.sh \
  SourceArt/Environment/grass_ground_source.png \
  Assets/Textures/Environment/grass_ground.png
```

This cropped strip is now retained only as a reproducible legacy source. The
current `menu.world` uses the opaque `menu_landscape_fullbleed.png` panorama,
which contains one continuous grass-and-soil layer and needs no repeated tiles.
