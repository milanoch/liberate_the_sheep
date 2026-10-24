# Кнопка-овечка — 2026-10-17

Новый рисунок создан встроенным imagegen, не взят из предыдущей игры.
Рабочий PNG: `Assets/Textures/UI/sheep_play_button.png`, 1536×1024, RGBA.
Метаданные/ID 3175671180 назначены редактором; вручную не редактировались.
В `menu.world` источник — Sprite `MenuPlayButtonArt`, за пределами камеры;
MenuController читает его текстуру и передаёт в PandaUI. Рисунок остаётся
обычным редактируемым ассетом в Content Browser.

Тело: UV-прямоугольник в пикселях (0, 0, 1536, 596).
Для размещения текста/плеча остаётся прежняя опорная высота 544: масштаб
картинки не меняется, новые ноги дорисовываются ниже. Высота контрола 216 pt.
Передняя лапка: (712, 570, 212, 440), плечо (810, 920).
Лапка масштабируется до 80% исходного размера относительно тела; крепится
в точке (1240, 425) тела, сбоку груди. Только она поворачивается, лицо и корпус неподвижны.
Жест: два плавных маха за 1.9 секунды, период 4.2 секунды с паузой;
огибающая sin² обнуляет скорость при начале/завершении жеста.
Базовый угол 1.65 rad, заданное отклонение до 0.38 rad дополнительно
смягчается огибающей; корень скрыт корпусом.
Выстриженные надписи — отдельные области того же PNG:
ИГРАТЬ (30, 696, 682, 238), PLAY (984, 696, 528, 238).
Выбор по локализации; центр на теле (768, 255), масштаб 84% относительно
пикселей корпуса. На самом рисунке тела букв нет. Нативный текст остаётся
резервом для отсутствующей текстуры или другого языка.

## Задняя пара: немного к хвосту и шире (2026-10-19)

После замечания пользователя исправлена только задняя пара ног: она
сдвинута к хвосту, между голенями и копытами оставлен более заметный зазор.
Сохранены длина, оттенок, небольшой сгиб и разница глубины ног.
Надписи, передняя опорная лапка, отдельная машущая лапка и раскладка атласа
сохранены. PNG заменён через built-in imagegen edit, без правок кода,
world, метаданных и анимации. Сборка не требуется.
Предыдущий рисунок: `SourceArt/UI/sheep_play_button_before_rear_leg_adjustment.png`.
Выбранный результат: `exec-1588b595-e4e8-45f9-9af5-a1eaf4abd840.png`.

### Промпт этой правки

Use case: precise-object-edit.
Input image 1 is the EDIT TARGET: the existing 1536x1024 transparent sheep-button atlas, not a reference for a new drawing.
Make ONE localized correction to the TWO attached HIND LEGS under the LEFT quarter of the long sheep. They are now too far toward the head and too close together. Move the pair moderately toward the LEFT/tail and increase their spacing. In source pixels, change leg centers from about x420 and x505 to about x310 and x440. Keep both hind legs under the rump, not directly under the tail. The new pair should have a visible natural gap between shins and hooves, roughly half a hoof-width, not touching or overlapping. Keep their current length, thickness, brown color, natural slight bends, and depth stagger. Their hooves must remain wholly above y=596. Heal the fleece only at the old and new attachment points. This is a modest reposition, not a redesign.
STRICT INVARIANTS: change only those two rear legs and their tiny wool attachment areas, approximately x=270..560,y=390..596. Preserve every other part of the atlas in its original place, size, colors and appearance: face, eyes, ears, expression, body, tail, the single resting FRONT LEG near x1160, and the entire bottom half with the detached waving limb and both existing sheared wool words "ИГРАТЬ" and "PLAY". Do not move or redraw the lower sprites. Do not add legs, lettering on the body, decorations, shadows, or any background. Do not resize or shift the sheep or canvas.
Preserve original true alpha transparency. The visible brown/black glow in RGB under transparent pixels must NOT become an opaque background: pixels outside the actual sprites remain alpha zero. Keep 1536x1024 RGBA. Output the edited atlas, not a composite screenshot.

## Выстриженная надпись и естественное крепление лапки (2026-10-19)

Через imagegen edit задняя пара ног сдвинута вперёд под круп. В свободных
нижних областях атласа добавлены русская и английская надписи из короткой
шерсти с ворсистыми краями. Раздельные надписи сохраняют переключение языка.
`MainMenuView.cpp` размещает их по центру корпуса вместо прежнего зелёного
текста, без дополнительной подложки. Область лапки сужена слева, чтобы не
захватывать соседние буквы; центр её вращения в исходнике не изменён.
Лапка крупнее и крепится ниже, мах направлен вбок от груди, а не из-под уха.
Не меняются размеры PNG, asset ID, world, меню уровней и фоновая анимация.
Предыдущий рисунок: `SourceArt/UI/sheep_play_button_before_sheared_title.png`.
Выбранный результат: `exec-9100b171-6d76-4c0d-b124-4cd3ac86b1a8.png`.
Результат в редакторе визуально оценивает пользователь после Reload.

## Расстановка и длина опорных ног (2026-10-19)

Через built-in imagegen edit изменён рисунок трёх опорных ног: задняя пара
слева под крупом, одна передняя справа под грудью, без ноги под серединой
живота. Голени заметно длиннее и тоньше, с небольшим естественным сгибом.
Четвёртая лапка остаётся отдельным машущим спрайтом; её UV и анимация прежние.
PNG 1536×1024 и asset ID 3175671180 сохранены, метаданные не правились.
Генерация дала копыта ниже прежних 544 px, поэтому область тела увеличена
до 596 px без растягивания рисунка и без захвата отдельной лапки ниже.
Предыдущий рисунок сохранён в `SourceArt/UI/sheep_play_button_before_leg_spacing.png`.
Выбранный результат: `exec-6fc4db0d-5a83-48e8-afa5-f4aa650acd52.png`.

## Исправление ориентации и крепления (2026-10-19)

`UIRenderer::drawRect` использует противоположное направление V относительно
координат PNG. Обе области получают UV `(x/W, (y+h)/H, w/W, -h/H)`:
переворот внутри выбранной области, без замены тела на другую часть атласа.
Сам PNG, импорт, world и текст надписи не переворачиваются и не изменяются.

Базовый угол лапки — 1.05 rad наружу, огибающая/скорость маха прежние.
Лапка рисуется перед корпусом (за ним по глубине), поэтому корень скрыт
шерстью, а копытце выходит сбоку от лица. Плечо на исходнике (810, 920)
остаётся неподвижным центром вращения.

## Исходный промпт (built-in imagegen)

Use case: stylized-concept.
Asset type: production 2D game UI sprite atlas, a friendly sheep-shaped PLAY button, transparent RGBA.
Create ONE 1536x1024 PNG sheet divided into TWO EXACT equal horizontal cells, each 1536x512. No borders, labels or text; transparent background everywhere outside artwork.
TOP CELL (y=0..511): one cute cream-white wool sheep with an intentionally very elongated horizontal pill-shaped wool body. Its tail is at left; its big friendly grey-brown face is at the right end but turned fully toward the viewer, with two eyes making eye contact, floppy ears and a gentle smile. The broad central-left wool body is uncluttered, bright cream, ready for an overlaid PLAY label. Three short relaxed legs/hooves below the body; OMIT the near foreleg entirely because it will be attached as a separate waving layer near the face. Fit the whole body+face+ears+three legs inside x=100..1430,y=65..450. Silhouette should look unmistakably like a sheep, not a sign or a rectangular panel. Warm illustrated farm-game art, softly modeled wool curls but crisp readable edges, natural taupe hooves, no black outlines, polished charming cartoon style.
BOTTOM CELL (y=512..1023): ONLY a detached matching sheep foreleg for rig animation, centered at x=768. The limb is upright: fluffy rounded cream shoulder attachment at its bottom near y=950, slender taupe leg extends upward to a rounded split hoof at y=585. This one limb occupies x=680..850 and y=570..960. It is the sheep's lifted foreleg as if waving hello, no human fingers, no additional body or face. Same art style and colors as top. Everything else in the bottom cell transparent.
Constraints: exactly these two separated sprite parts, no scene, no ground shadow, no glow, no text, no checkerboard background, no other sheep, do not draw the omitted waving leg on the top sheep. Preserve the exact two-row arrangement.

## Очистка прозрачного фона (built-in imagegen edit)

Use case: background-extraction.
Edit the provided sheep button atlas. Preserve the sheep artwork, face, long fluffy body, three resting legs and the detached raised foreleg, as well as all positions, sizes and composition exactly.
Remove ALL background, brown/black/cream gradients, glow, cast shadow, haze and halos around BOTH sprite parts. Pixels outside the actual sheep silhouette and outside the actual detached foreleg silhouette must be fully transparent, alpha zero. Keep clean crisp antialiased silhouette edges and the illustrated shading INSIDE the sheep only. No added content. Do not move, resize, redraw or crop any part. Keep 1536x1024 canvas. Genuine transparent PNG for layering over a game scene.

Первый результат: `exec-2e8071dc-c7d1-4c9f-8deb-036dbea10571.png`.
Выбран результат edit: `exec-ecbe3449-433c-4dae-99f9-9be2d204713b.png`.
Сохранённый RGBA используется без растеризации в коде и без изменения пикселей.

## Промпт правки ног (built-in imagegen edit)

Use case: precise-object-edit.
Input image: edit target, existing production UI sheep sprite atlas.
Change ONLY the three resting legs attached beneath the long sheep in the TOP part of this atlas. Their current equal spacing like table legs is wrong.
Replace them with anatomically sensible placement: TWO hind legs close together beneath the rump near the LEFT end (roughly x=245 and x=355), and ONE resting far foreleg beneath the chest/neck near the RIGHT end (roughly x=1230). The near foreleg is waving and is already a DETACHED sprite below; do NOT add another fourth resting leg. There must be a long clear leg-free gap beneath the middle of the belly. Exactly THREE resting hooves on the body.
Make the resting legs visibly longer and slimmer, with visible warm taupe/brown shins and natural subtle knee/hock bends, ending in small split dark hooves. Hind pair has slight depth stagger, the far hind leg partially hidden behind the near one. Not three vertical stubby wool columns. Front resting leg starts beneath the chest around y=360-385; rear pair starts beneath rump around y=380-405. Hooves end around y=530, entirely above y=544. Locally trim only the little wool cuffs where legs emerge, so more shin shows; keep the main wool belly and the huge fluffy button body unchanged. Match original painterly cartoon style, shading, and brown palette. Do not draw human arms.
CRITICAL INVARIANTS: Keep exact 1536x1024 canvas, body size and position, head, face, eyes, expression, ears, tail, wool texture, color and lighting unchanged. Keep the entire separate raised foreleg in bottom region x=700..924,y=570..1010 absolutely unchanged in appearance, size, orientation and position: it is already animated by code. Do not move any non-leg artwork. No lettering: PLAY is overlaid in code. Preserve true transparent alpha outside the sprites, no new background, shadow or halo. Only change the three attached legs and their tiny wool attachment areas.

## Промпт выстриженных букв и сдвига задних ног (built-in imagegen edit)

Use case: precise-object-edit.
Input image is the EDIT TARGET: an existing transparent 1536x1024 game UI sprite atlas. Keep its canvas, sheep's face, smile, eyes, ears, wool body, tail, overall proportions, front resting leg, and the detached foreleg in the lower middle unchanged.
Make these precise changes only:
1. Move BOTH rear resting legs (currently at left near x250 and x340) roughly 115 pixels TOWARD THE HEAD, to centers around x365 and x455. Keep their length, subtle bends, depth staggering and count. Heal wool at their previous attachment points. They must be a close hind pair under the hindquarter, not under the tail edge; no middle leg and no extra legs.
2. In the UNUSED LOWER LEFT of the atlas, add one separate transparent overlay sprite of the exact Cyrillic word "ИГРАТЬ" (six letters: И Г Р А Т Ь), centered within x=40..660, y=735..915. In the UNUSED LOWER RIGHT add one separate transparent overlay sprite of the exact English word "PLAY", centered within x=975..1495,y=735..915. These are localized title decals, not additional buttons. DO NOT put any lettering on the sheep itself.
Both word sprites must look like letters freshly CLIPPED INTO CREAM SHEEP WOOL: wide readable rounded uppercase glyphs filled with fine short golden-beige stubble, recessed soft amber inner edges and a very thin irregular cream-fiber fringe matching the sheep. A shallow sheared depression, NOT raised foam letters, wooden letters, ink, green paint, or a stitched patch. Only the individual letter shapes and tiny fiber edges are opaque. Gaps between letters, holes inside letters, and all surrounding area MUST stay fully transparent. NO backing rectangle, cloud, banner, oval or drop shadow behind either word. Keep enough contrast for the sheared text to read when overlaid onto the fluffy cream body.
3. CRITICAL: keep the separate raised foreleg at x700..924,y570..1010 in EXACT same place, dimensions, colors and pose, because code animates it; do not join it to either word. All top sheep pixels must remain within y0..596 (resting hooves uncut); bottom word sprites and limb remain separate.
Maintain true alpha transparency outside the sprites. Do not turn transparent RGB bleed into an opaque brown background. Keep the polished warm cartoon illustration style and original lighting. No other changes.
