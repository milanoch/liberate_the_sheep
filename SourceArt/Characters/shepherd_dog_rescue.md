# Пастушья собака: спасение от бомбы

## Актуальная версия v5: заметный шаг и непрерывная фаза (2026-11-02)

В игре фактически оставался атлас v3: его SHA совпадал с runtime PNG, поэтому
подготовленный в v4 хвост не был виден. v4 не был доведён до установки;
описание этой подготовки ниже не означает, что его рисунок использовался игрой.

Новый исходник `shepherd_dog_run_v5_source.png` создан встроенным imagegen
по `shepherd_dog_run_v5_reference.png` (16 поз, crop ключей v4). Промпт:

> Preserve this exact volumetric overhead Border Collie, face, coat, lighting,
> body dimensions and 4×4 equal-cell layout. Make all four white-tipped paws
> visibly articulate in a natural fore-and-aft canter with substantially larger
> travel, about 20–25 pixels in a 256-pixel cell. Forepaws reach beside the
> lower neck, plant and push back; hind paws gather beneath hips then extend
> behind for push-off. Narrow canine limbs, not sideways paddling or spider legs.
> Two sequential eight-phase strides: hind push-off, tucked suspension, leading
> forepaw reach/contact, other forepaw contact, weight absorption, rear recovery,
> staggered rear touchdown and extension. Opposite lead in the second stride.
> Subtle shoulder weight transfer and spinal compression, stable head and torso.
> Keep one visible gentle tail cycle across sixteen poses: centre → right →
> centre → left → centre, fixed tail root. Bottom-row tails return from left,
> never jump right. Same size and camera, transparent padding, no shadow or text.

Все 16 поз зарегистрированы одним масштабом 0,76, пасть в (128,36).
`pack_shepherd_dog_run_v5.swift` и `shepherd_dog_run_v5_registration.json`
сохраняют импорт. `build_shepherd_dog_inbetweens.swift --dense` выпускает
192 кадра бега: каждый ключ и 11 промежуточных поз, включая переход 15→0.
Это два шага и один цикл хвоста на прежние 8,4 клетки пути, а не удвоение
скорости воспроизведения. Объёмные захват и прыжок сохранены из v3.

Атлас v5: **3840×3584**, сетка **15×14**, ячейка 256×256. Кадры 0–191 — бег,
192–203 — захват, 204 — прыжок, 205–209 — прозрачный резерв. Чистовой файл
`shepherd_dog_rescue_v5_atlas.png` предназначен для игрового
`Assets/Textures/Characters/shepherd_dog_rescue.png`, ID 745241933 прежний.
Обе authored сущности в game.world и ShepherdDogAnimation.hpp используют 15×14.
CRT-адресация Panda всё ещё нужна после обновления SDK: столбец index%cols,
строка index%rows. 15 и 14 взаимно просты.

Исправлен источник скачка после прыжков: gaitDistance больше не считает
путь в воздухе, пока показана собранная поза. После приземления фаза шага
продолжается, не перескакивая через скрытые кадры и не наверстывая их ускоренно.
Разгон/торможение на земле по-прежнему согласуют частоту лап с перемещением.
Размер 2,4, привязка пасти 0,8625 и тень только в прыжке сохранены.

Чистовой v5 установлен в игровой PNG; SHA обеих копий совпадает.
Рабочая библиотека скриптов обновлена (только target liberate_the_sheep, exit 0).
Тесты, тестовые сборки, редактор и игра не запускались. После человеческого
Stop → Reload (Cmd+R) → Play оценить лапы и хвост на земле, продолжение шага
после прыжка и перенос бомбы у пасти.

Команды производства:

```sh
swift -module-cache-path /private/tmp/shepherd-dog-v5-swift-cache SourceArt/Characters/pack_shepherd_dog_run_v5.swift register SourceArt/Characters/shepherd_dog_run_v5_source.png SourceArt/Characters/shepherd_dog_run_v5_registration.json 0.76
swift -module-cache-path /private/tmp/shepherd-dog-v5-swift-cache SourceArt/Characters/pack_shepherd_dog_run_v5.swift prepare SourceArt/Characters/shepherd_dog_run_v5_source.png SourceArt/Characters/shepherd_dog_run_v5_registration.json SourceArt/Characters/shepherd_dog_rescue_v3_atlas.png SourceArt/Characters/shepherd_dog_rescue_v5_keyframes.png SourceArt/Characters/shepherd_dog_jump_v5.png
swiftc -O -module-cache-path /private/tmp/shepherd-dog-v5-swift-cache SourceArt/Characters/build_shepherd_dog_inbetweens.swift -o /private/tmp/shepherd-dog-v5-baker
/private/tmp/shepherd-dog-v5-baker SourceArt/Characters/shepherd_dog_rescue_v5_keyframes.png SourceArt/Characters/shepherd_dog_rescue_v5_baked.png SourceArt/Characters/shepherd_dog_jump_v5.png --dense
swift -module-cache-path /private/tmp/shepherd-dog-v5-swift-cache SourceArt/Characters/pack_shepherd_dog_run_v5.swift finish SourceArt/Characters/shepherd_dog_rescue_v5_baked.png SourceArt/Characters/shepherd_dog_rescue_v3_atlas.png SourceArt/Characters/shepherd_dog_rescue_v5_atlas.png
```

## Подготовка v4: хвост, спокойные лапы, размер и тень (2026-11-01)

По следующему отзыву пользователя сохранена объёмная собака, но бег получил
видимый цикл покачивания хвоста и более последовательные движения лап.
16 ключей охватывают два шага и одно покачивание хвоста; между ними офлайн
создаются 80 промежуточных кадров. Захват и прыжок берутся из объёмного v3,
без возвращения старого плоского рисунка.

Новые ключи созданы встроенным imagegen по `shepherd_dog_run_v2_source.png`:

> Edit only the tail and four paws of this exact approved volumetric overhead
> Border Collie. Preserve head, white ruff, rounded shoulders and haunches,
> fur lighting, scale and torso registration. A 4×4 sheet, two sequential
> eight-phase easy canter strides: hind paws gather and push off, tucked
> suspension, staggered forepaw reach and contact, weight absorption, recovery.
> Feet move fore-and-aft beneath the body, not sideways. One visible gentle
> tail cycle across all sixteen keys: centre, right, centre, left, centre.
> Tail root fixed; no whole-body sway. Transparent padding, no cast shadow.

Возврат хвоста потребовал отдельной вставки из четырёх поз. Промпт вставки:

> Four poses only, in a 2×2 transparent sheet, matching this exact volumetric
> Border Collie. Preserve coat, camera and body proportions. Tail returns
> from lower-left toward centre: moderate left, slight left, almost centre,
> centre. White tip remains left of the spine until centred; never suddenly
> jumps right. Paws finish an easy canter stride with staggered landing,
> absorption and hind-paw recovery. Body registered, no whole-body sway.

Используются первые 12 поз `shepherd_dog_run_v4_source.png` и четыре позы
`shepherd_dog_tail_return_v4_source.png`. Их упаковка и привязки сохранены
в `pack_shepherd_dog_run_v4.swift` и JSON регистрации. После интерполяции
в чистовой `shepherd_dog_rescue_v4_atlas.png` переносятся точные ячейки действий
96–108 из v3. Игровое имя и ID 745241933 остаются прежними.

Квад собаки увеличен с 2 до 2,4 клетки (+20%). Смещение пасти теперь выводится
из её привязки (128,36) и размера квада: 0,8625 клетки. Оно одинаковое при
подходе к овечке и переносе бомбы. Шаг увеличен с 3,05 до 4,2 клетки: при
прежней скорости 8,25 клетки/с лапы двигаются примерно на 27% медленнее.
Радиус обхода увеличен пропорционально до 0,324 клетки.

При беге, захвате и развороте альфа тени строго равна нулю. В прыжке она
плавно появляется выше lift=0,035, с низкой альфой не более 0,11 и дополнительным
ослаблением на высоте. Игру и тесты агент не запускает; окончательное принятие
движения после человеческого Stop → Reload → Play.

## Версия v3: единый рисунок во всех действиях (2026-11-01)

После принятия нового бега пользователь заметил, что прыжок и захват всё ещё
используют старую плоскую собаку. Эти 13 ячеек заменены новыми позами той же
объёмной собаки. Беговые кадры 0–95 сохраняются из v2 без перерисовки.
Игровой код, скорость, путь, траектория прыжка, длительность захвата и момент
контакта пасти с бомбой не меняются. Новый рисунок сохраняет привязку пасти
к (128,36) в ячейке 256×256 и масштаб бегущей собаки.

Создано встроенным imagegen, режим редактирования с единственным эталоном
`shepherd_dog_run_v2_source.png`. Промпт:

> Use case: identity-preserve. Create a matching action animation sheet for
> the exact approved volumetric Border Collie. Preserve proportions, rounded
> shoulders and haunches, narrow white facial blaze, sculpted white neck ruff,
> black fur highlights, camera and lighting. Strict overhead, nose up, same
> registration and scale in every cell. Exactly 4 columns by 4 rows.
> First twelve poses: settle, plant forepaws, lower head, reach muzzle, open
> jaws slightly, close on an invisible strap, tighten grip, lift muzzle,
> withdraw neck, shift weight back, recover, ready to run carrying. Subtle
> continuous canine motion, tail calm and nearly straight behind.
> Last four poses: identical compact airborne dog with paws tucked beneath
> chest and hips, same body proportions and volume. No bomb or strap drawn.
> Full transparent padding, no clipping, coloured fringe, shadow, grid or text.

Исходник: `shepherd_dog_actions_v3_source.png`. Привязки и общий масштаб:
`shepherd_dog_actions_v3_registration.json`. Импортёр:
`pack_shepherd_dog_actions.swift`. Чистовой атлас:
`shepherd_dog_rescue_v3_atlas.png`, копируется в
`Assets/Textures/Characters/shepherd_dog_rescue.png`.
Сетка 13×9 и ID 745241933 прежние: 96–107 — новый захват, 108 — новый прыжок,
109–116 — прозрачный резерв. Старые v1/v2 ниже — только история и исходники;
их нельзя напрямую копировать поверх текущей игровой текстуры.

Воспроизводимая команда импорта новых действий поверх принятого бега:

```sh
swift -module-cache-path /private/tmp/shepherd-dog-actions-v3-swift-cache SourceArt/Characters/pack_shepherd_dog_actions.swift SourceArt/Characters/shepherd_dog_rescue_v2_atlas.png SourceArt/Characters/shepherd_dog_actions_v3_source.png SourceArt/Characters/shepherd_dog_actions_v3_registration.json SourceArt/Characters/shepherd_dog_rescue_v3_atlas.png
```

Импорт завершён, чистовой v3 установлен в игровую текстуру. Сборки скриптов,
тесты, редактор и игра для этой правки не запускались. После Reload вручную
оценить переходы бег → прыжок → бег и бег → захват → бег с бомбой.

## Версия v2: объём и спокойный бег (2026-11-01)

Пользователь принял прыжки и захват, но попросил более объёмную собаку,
плавные лапы и спокойный хвост. Для бега создан новый рисунок через встроенный
imagegen: округлые плечи, грудная клетка и бёдра, свет на чёрной шерсти,
более отчётливые белые лапы. Хвост направлен назад, без прежних широких взмахов.

Промпт нового рисунка:

> Beautiful volumetric 3D-rendered black-and-white Border Collie seen strictly
> overhead, muzzle up. Rounded raised shoulders, solid ribcage, muscular
> haunches, sculpted white ruff; soft silver highlights and ambient occlusion
> on black fur. Exactly sixteen registered poses in a 4×4 grid: two complete
> eight-phase light canter strides. Four canine paws move forward and back
> with natural elbow/hock bending, hind push-off, forepaw touchdown and weight
> absorption. Same identity, size and camera across all poses. Tail held almost
> straight behind the dog, with only a small relaxed tip movement; no wide or
> rapid wagging. Transparent background and padding, no text, grid or objects.

Последующее уточнение imagegen сохранило позы и объём, задав нейтральную
чёрно-белую шерсть и очистку цветной бахромы.

Файлы исходников и сборки:

- `shepherd_dog_rescue_v1_accepted.png` — сохранённый предыдущий атлас.
- `shepherd_dog_rescue_v2_keyframes.png` — 16 новых ключей бега и 12 прежних
  ключей захвата, приведённые к одинаковым ячейкам 256×256.
- `shepherd_dog_jump_v1_accepted.png` — прежняя принятая поза в воздухе.
- `pack_shepherd_dog_v2.swift` — упаковка и привязка корпуса/пасти.
- `build_shepherd_dog_inbetweens.swift` — офлайн интерполяция движения через
  Vision optical flow и контурную маску signed distance. Промежуточные позы
  двигают исходные пиксели; прозрачные двойные лапы не используются.

Чистовой атлас теперь **3328×2304**, **13×9** ячеек по **256 px**:
0–95 — бег (два шага), 96–107 — прежний захват, 108 — прежняя поза прыжка,
109–116 — прозрачный резерв. `ShepherdDogAnimation.hpp` задаёт этот контракт.
ID текстуры **745241933** и имя игрового PNG сохраняются.

Фаза бега считается от пройденного расстояния на два шага (6,10 клетки).
Дополнительные кадры не ускоряют лапы; разгон и торможение по-прежнему
замедляют их вместе с собакой. Скорость маршрута, траектории прыжков, момент
захвата, перенос бомбы и карточка не изменены. Контактная тень чуть выразительнее.

Команда создания игрового атласа после упаковки:

```sh
swift -module-cache-path /private/tmp/shepherd-dog-swift-cache SourceArt/Characters/build_shepherd_dog_inbetweens.swift SourceArt/Characters/shepherd_dog_rescue_v2_keyframes.png SourceArt/Characters/shepherd_dog_rescue_v2_atlas.png SourceArt/Characters/shepherd_dog_jump_v1_accepted.png
```

Атлас выпущен и скопирован в `Assets/Textures/Characters/shepherd_dog_rescue.png`.
Утилита выбирает поддерживаемую системой ревизию Vision optical flow; здесь
использована Revision 1. Рабочая библиотека скриптов также обновлена.
Игровая анимация оценивается пользователем после Stop → Reload → Play.

Историческое описание первой версии ниже. Старые 4×7 и 28 кадров относятся
только к ней; для текущей игровой текстуры их не использовать.

## Первая версия

Создано встроенным `image_gen`, 2026-11-01. Новый персонаж — проворный
чёрно-белый бордер-колли в рисованном стиле стада.
Исходник с прозрачностью: `shepherd_dog_rescue_source.png` в этой папке.
Игровой атлас: `Assets/Textures/Characters/shepherd_dog_rescue.png`.
Чистовая копия: `shepherd_dog_rescue_clean_atlas.png`. Итоговый размер
948×1659, ячейки 237×237, расположение центра корпуса нормализовано в (0.5,0.5).
ID текстуры выдал редактор: **745241933**; `.pmeta` вручную не изменять.

## Промпты рисунка

Основной запрос (built-in imagegen, transparent background):

> Production 2D top-down game character animation spritesheet. Exactly four
> columns and seven rows, 28 poses. Strict orthographic overhead view, dog muzzle
> up and tail down. Agile athletic Border Collie sheepdog, black sable saddle,
> white neck ruff, narrow white facial blaze, warm tan eyebrow accents, white
> paws, glossy dark nose, semi-pricked ears, flowing bushy white-tipped tail.
> Detailed softly painted cartoon realism matching a hand-painted farming game,
> natural anatomy, crisp edges, subtle form shading, no black outline.
> First 16 poses: one sequential seamless light canter cycle — gathering,
> hind-paw push-off, suspension, forepaw landing and absorption. Narrow natural
> legs move forward/back, never splay sideways. Small spinal flexion, shoulder
> weight transfer, steady head, ears following motion, one relaxed tail wag.
> Last 12 poses: brake, plant paws, lower and reach muzzle to an invisible strap,
> close jaws, pull back and lift into a carrying pose. No bomb drawn into image.
> Keep identity, coat, scale, torso registration and camera consistent. Full
> transparent padding, no text, grid lines, scenery, cast shadows or extra objects.

Уточнение первого результата:

> Preserve the same character and overhead view. Make all four white-tipped paws
> visibly articulate in sequential canter phases beneath shoulders and hips;
> no sideways spider limbs. Give the dog natural shoulder and spine motion,
> preserve coat identity, and keep all 28 poses in the same 4 by 7 layout.

Уточнение полей:

> Preserve the exact same dog and 28 poses. Reduce individual figures inside
> their cells, keep complete tails and paws, add generous transparent padding,
> and register each torso consistently. No redesign, shadows, text or drawn grid.

Генерация не дала строго равномерных межстрочных полей. Поэтому исходные позы
упакованы импортным Swift-скриптом в равные ячейки: перенос готовых пикселей с
привязкой корпуса, без перерисовки, растяжения конечностей или подмены анимации.
Использовать игровой атлас, а не исходник, в Sprite.

После упаковки 1024×1792 выполнена ещё одна правка через imagegen. Промпт:

> Edit the registered 4 by 7 spritesheet into a strictly achromatic black and
> white Border Collie. All visible fur pixels must be neutral charcoal, white
> or silver grey. Remove coloured noise and detached halo specks to transparent
> alpha. Preserve all 28 poses, equal cell layout, relative positions, scale,
> overhead camera, complete tails and paws. Do not redesign or add objects.

Чистовой результат уменьшен генератором пропорционально до 948×1659, поэтому
ячейки остаются квадратными и нормализованные привязки не меняются.
`pack_shepherd_dog.swift` теперь сохраняет только промежуточный цветной атлас;
для восстановления игрового файла скопировать `shepherd_dog_rescue_clean_atlas.png`,
а не результат повторной упаковки исходника.

## Отображение и движение

- Сетка 4×7: 16 кадров бега и 12 захвата. Panda адресует строку и столбец через
  остатки от деления; использовать `ShepherdDogAnimation::encodedCell`.
- `ShepherdDogRescuer` и `ShepherdDogShadow` заранее созданы в `game.world`.
  Тень использует ту же прозрачную форму, слабый чёрный tint и позицию на земле.
- `ShepherdDogRescue` ведёт собаку по клеточным координатам с разгоном,
  торможением и поворотами. Фаза лап зависит от расстояния, а не от FPS.
- При захвате бомба остаётся на овечке до контакта пасти; затем тот же Sprite
  переносится к пасти, его дисплей гаснет, собака возвращается за экран.
- Бегущие овечки тоже учитываются как препятствия, хотя они уже удалены из
  логической сетки. В плотном стаде предусмотрены короткие прыжки при отсутствии
  доступного прохода; тень остаётся на земле.
- В полностью сомкнутом стаде, где невозможно даже приземление, овечки плавно
  дают собаке место и возвращаются обратно. Это только временное визуальное
  смещение: игровая сетка, порядок освобождения и расстановка не меняются.

## Игровые правила

Срабатывает только при включённом бессмертии: таймер красной дошёл до нуля
или закончились разрешённые удары чёрной. На время спасения поле, бегущие
овечки и остальные таймеры приостановлены. После ухода собаки игра продолжается
с теми же овечками; снятая бомба больше не активируется. Каждая бомба спасается
отдельно. В обычном режиме остаётся прежнее поражение от взрыва.

Карточка на 3,8 секунды плавно появляется и исчезает, без затемнения поля:
«Бессмертный режим включён — собака снимет бомбу и унесёт её подальше от стада».
Есть русский и английский варианты. Перезапуск/выход очищает собаку и карточку.

Игра и тесты агентом не запускались. Визуальное принятие движения, захвата и
масштаба — после человеческого Stop → Reload → Play.
