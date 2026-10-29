# Красная и чёрная бомбы на овечках

2026-10-29: PNG обновлён на двухколоночный атлас1774×887. ID2523272276
сохранён. Левая квадратная ячейка — красная бомба с таймером, правая —
чёрная со счётчиком ударов. В game.world cols=2, rows=1; код выбирает
ячейку0/1. Новый ресурс регистрировать не требуется. Оригинальная одиночная
красная картинка сохранена как `sheep_bomb_red_original.png` рядом с этим файлом.

Атлас получен встроенным imagegen с исходной красной бомбой как edit target.
Финальное задание: exact2:1 transparent two-column sprite atlas; preserve the
red original in the left square; make an identical charcoal-black body variant
in the right square, retaining warm straps, brass/ivory screen rim and coloured
loops. Both objects share scale, pose, silhouette and transparent padding.
Empty dark screens; no digits, lettering, sheep, scenery, borders or watermark.

У текущего атласа центр дисплея в каждой ячейке (.503,.560). UI показывает
красный MM:SS или одну крупную светлую цифру, следуя положению/повороту Sprite.
Для чёрной бомбы число означает, на каком по счёту ударе она взорвётся:
3→2→1→взрыв. В бессмертии на0 гаснет без поражения. Карточка соответствующей
механики показывается при каждом входе в уровень.

Ниже сохранено описание предыдущей одиночной версии красной бомбы.

2026-10-29: ресурс зарегистрирован редактором с ID2523272276 и подключён
в `game.world` к `BombSheepAccessory`. Привязка завершена. Карточка механики
показывается при каждом запуске уровня с бомбой, включая повторную игру.

`Assets/Textures/Gameplay/sheep_bomb.png` — отдельный PNG с прозрачным фоном,
созданный через imagegen 2026-10-28 для механики уровней 11–24. Исходные овечки,
их цвета и атласы бега не изменяются. Бомба следует за выбранной овечкой как
Sprite `BombSheepAccessory` из `Assets/Worlds/game.world`.

Обновление по референсам пользователя: три красные мультяшные шашки,
тёмные ремешки и крупный цифровой дисплей с тёплой светлой рамкой.
Новый PNG создан встроенным imagegen с настоящей прозрачностью;
предыдущая круглая бомба заменена по запросу пользователя.

Финальное задание для imagegen: production 2D cartoon game sprite,
three thick rounded red cylinders running horizontally, two dark brown
straps, central dark digital timer screen with an ivory/brass rim,
short playful coloured wire loops. Orthographic view with no perspective
distortion, warm softly shaded game art and crisp readable silhouette.
Single compact object on transparent alpha; no sheep, background, text,
numbers, logos, watermark or screenshot UI. User references guide the
bundle and display, without copying their banners or other surrounding UI.

Картинка 1280×1280. Внутренний экран расположен около x=.29…69,
y=.45…60; центр (.491,.526). `BombSheepView` рисует красные семисегментные
цифры MM:SS непосредственно в этом экране, одним способом для карточки
и поля. На поле положение, размер и поворот совпадают со Sprite бомбы;
отдельной плашки над овечкой нет. Размер Sprite — 1.16 клетки, смещение
по спине — -.12 клетки. Таймер дублируется в HUD для чтения на малом поле.

Карточка знакомства использует тот же PNG поверх существующего спрайта
стоящей овечки соответствующего цвета. Для взрыва используется короткая
анимация дыма и вспышки средствами PandaUI. В бессмертном режиме бомба
безопасно гаснет на нуле, овечка продолжает участвовать в уровне.

Для регистрации нового файла нужна кнопка Refresh в панели Content Browser:
она вызывает rescanProject(). Обычный Reload сам по себе не зарегистрировал
новую папку Gameplay. После рескана ID берётся из созданного редактором
`sheep_bomb.png.pmeta` и привязывается в game.world. Метаданные вручную
не создавать. Для уже привязанной изменённой картинки достаточно Reload.
