# Side-view sheep rig source

Generated with the built-in imagegen tool, 2026-09-09. Style reference:
`sheep_side_run_16_source.png`. Output: `sheep_side_rig_parts.png`.

The generator returned an opaque checkerboard despite the transparency request.
The offline atlas baker removes this neutral background before rendering.

## Generation prompt

Use case: stylized-concept. Asset type: production 2D sheep cutout animation parts sheet on genuine transparent background.
Image 1 is STYLE reference ONLY for the existing friendly fluffy sheep. Create clean separate body parts for animating the same type of animal running SIDE VIEW, always facing RIGHT. Painterly cream curly wool, dark warm grey face and legs, small brown cloven hooves, warm detailed storybook illustration. Slightly more natural ovine proportions, not a round ball.
Layout: exactly SIX isolated parts in a regular invisible THREE COLUMNS by TWO ROWS grid. Each part fully contained within its cell, centered with generous transparent padding; no visible grid or labels. Landscape canvas 1536x1024.
TOP LEFT: just the horizontal oval fluffy torso, wool only, NO HEAD, NO NECK, NO LEGS, NO TAIL.
TOP MIDDLE: head with attached short fluffy neck, a SINGLE sheep head in clean right-facing profile, one visible dark bright eye, short grey muzzle pointing right, two small ears, curly white forehead; neck ending lower left to attach to torso.
TOP RIGHT: small short fluffy sheep tail extending horizontally leftward, rounded joint on its right end.
BOTTOM LEFT: one detached straight FRONT UPPER LEG SEGMENT only, vertical, shoulder at top, knee at bottom; rounded ends, charcoal warm grey hair, NO hoof. A small wool cuff on top only.
BOTTOM MIDDLE: one detached straight HIND UPPER LEG/THIGH SEGMENT only, vertical, slightly thicker, rounded hip top and hock bottom, charcoal grey fur, NO hoof.
BOTTOM RIGHT: one detached straight LOWER LEG WITH HOOF, vertical thin charcoal grey shin with rounded knee at top, dark brown cloven hoof at bottom pointing a little right.
All six parts same character, consistent top-left soft lighting, finely painted edges. NOT six whole sheep. No ground, shadow, scenery, text, watermark, checkerboard, white backdrop, extra detached objects. Preserve actual alpha transparency. This is an animation puppet asset, each joint must overlap cleanly.

## Transparency retry prompt (output not selected)

Use case: background-extraction. Image1 edit target. Remove ONLY the gray and white checkerboard background and make it genuinely transparent (alpha=0), including the gaps around the six detached sheep animation parts. Keep every sheep part unchanged in its exact position and size, its detailed warm wool and dark face and legs. No checkerboard pixels baked into image. Output transparent PNG, not a checkerboard simulation. Maintain 1536x1024 dimensions.

