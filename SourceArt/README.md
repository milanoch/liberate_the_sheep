# Source art

This directory contains editable/generator source images. PandaEditor does not
load these files at runtime; the imported, optimized atlases live under
`Assets/Textures/Characters/` and retain their editor-issued `.pmeta` IDs.

Rebuild both sheep atlases with:

```sh
Scripts/liberate_the_sheep/scripts/build_sheep_animation.swift \
  SourceArt/Characters/sheep_side_run_16_source.png \
  SourceArt/Characters/sheep_side_jump_24_source.png \
  SourceArt/Characters/sheep_topdown_white_source.png \
  Assets/Textures/Characters/sheep_side_run.png \
  Assets/Textures/Characters/sheep_topdown.png
```

The builder removes generated checkerboard backgrounds, aligns every side-view
pose by its wool-torso anchor, and derives black and brown coats from the same
white masters so all three colors keep pixel-identical silhouettes and pivots.
