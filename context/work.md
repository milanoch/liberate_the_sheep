# Рабочий контекст: liberate_the_sheep

Актуализировано: 2026-08-26. Это проект на собственном движке Panda, не Panda3D.

## Архитектурные правила игры

- Каждый отдельный уровень хранить в собственном `.world`.
- Главное меню также хранить в отдельном `.world`; стартовая конфигурация готовой
  игры должна открывать именно мир меню.
- Если объект может существовать уже при старте world, создавать его в `.world`, а
  не из C++. Так геометрию, трансформы, компоненты, связи и параметры можно менять в
  PandaEditor без перекомпиляции.
- Из кода создавать только действительно динамические/временные/процедурные объекты.
- Контроллер уровня должен находить заранее созданные объекты по тегам или получать
  ссылки через опубликованные `EntityHandle`-поля.
- Переходы между меню и уровнями делать через опубликованные `WorldHandle`-поля и
  `Bamboo::WorldAPI::load()`. Идентификаторы миров задаёт редактор.
- Для меню/HUD: постоянный сценовый фон, камеры, свет и декорации живут в world;
  интерактивную интерфейсную иерархию удобно собирать через PandaUI.

## Источники правды

- Инструкции проекта: `CLAUDE.md`.
- Проектные скиллы:
  - `.claude/skills/panda-world-editing/SKILL.md`;
  - `.claude/skills/panda-script-fields/SKILL.md`;
  - `.claude/skills/panda-assets/SKILL.md`;
  - `.claude/skills/panda-build-scripts/SKILL.md`.
- SDK: `/Applications/PandaEditor.app/Contents/Resources/PandaSDK`.
- Документация SDK:
  - `docs/sdk-overview.md`;
  - `docs/world-format.md`;
  - `docs/assets.md`;
  - `docs/scripting.md`.
- Точные сигнатуры API всегда сверять с текущими заголовками `PandaSDK/include/`,
  прежде всего `Bamboo/` и `Panda/ScriptEngine/ABI/`.
- Полезные проекты-примеры: `/Users/milan/PandaProjects/PandaExamples`.

## Рабочий цикл

1. Если PandaEditor открыт, перед внешней правкой попросить пользователя сохранить
   несохранённые изменения. Иначе Reload может показать конфликт disk/editor.
2. Миры и контент править в `Assets/`, C++ — в
   `Scripts/liberate_the_sheep/src/`.
3. После изменения C++ самостоятельно конфигурировать/собирать скрипты:

   ```sh
   PANDA_SDK_DIR=/Applications/PandaEditor.app/Contents/Resources/PandaSDK/lib/cmake/PandaSDK \
     Scripts/liberate_the_sheep/scripts/configure_scripts.sh debug
   PANDA_SDK_DIR=/Applications/PandaEditor.app/Contents/Resources/PandaSDK/lib/cmake/PandaSDK \
     Scripts/liberate_the_sheep/scripts/build_scripts.sh debug
   ```

   Конфигурация нужна один раз, если существует совместимый CMake cache; сборка —
   после каждой правки C++.
4. Редактор и игру запускает только пользователь. Перед загрузкой новой скриптовой
   библиотеки Play/Simulation должен быть остановлен.
5. В конце попросить пользователя нажать Reload (`Cmd/Ctrl+R`) и проверить игру.
6. После Reload редактор обновляет `.Panda/scripts-manifest.json`; только после этого
   привязывать опубликованные поля скриптов в world.

## Правила `.world`

- `.world` — JSON. Проверять синтаксис после каждой ручной правки.
- Обязательные ключи сущности: `id`, `tagComponent`, `relationshipComponent`,
  `transformComponent`, `scriptListComponent`.
- `id` — уникальный ненулевой случайный `uint32`; не переиспользовать удалённые ID.
- `rotation` в файле — quaternion `{x,y,z,w}`; identity — `{0,0,0,1}`.
- Иерархия двусторонняя: родитель содержит ребёнка в `children`, ребёнок содержит ID
  родителя в `parent`. У корня `parent: 0`; трансформ ребёнка локальный.
- Для неизвестного компонента сначала создать пример в PandaEditor, сохранить world и
  использовать полученный JSON как схему. Установленный SDK пока не содержит
  упомянутый документацией `Panda/Serialization/ComponentDtos.hpp`.
- Ссылки на entity используют ID сущности того же world. Ссылки на texture/material/
  world используют ID ассета, назначенный редактором.
- После правки проверять: валидный JSON, уникальность ID, существование parent/children,
  согласованность обеих сторон иерархии и корректность всех ссылок.

## Скрипты и поля

- Скрипт наследуется от `Bamboo::Script` и регистрируется `REGISTER_SCRIPT(Class)`.
- Доступный lifecycle: `start`, `update`, `lateUpdate`, collision/sensor begin/end,
  `shutdown`.
- Публичные inspector-поля объявляются через `PANDA_FIELDS_BEGIN`, `PANDA_FIELD`,
  `PANDA_FIELDS_END`.
- Поддержанные типы полей: `int`, `float`, `EntityHandle`, `TextureHandle`,
  `MaterialHandle`, `WorldHandle`.
- Значения по умолчанию обязательно инициализировать в C++; world может не содержать
  override для поля.
- `fieldId` никогда не вычислять вручную: брать из актуального
  `.Panda/scripts-manifest.json` после успешной сборки и Reload.
- Числовые типы в world: `0` int, `1` float, `2` entity, `3` texture,
  `4` material, `5` world.
- API мира, сущностей, компонентов, ассетов и PandaUI — main-thread-only.
  `Bamboo::Async::work` выполняет только вычисления над копиями/plain data;
  результат применяется через `complete` на main thread. Не захватывать живой `this`
  в долгую фоновую работу.
- В `shutdown()` освобождать runtime-ресурсы и очищать PandaUI root, если скрипт им владеет.

## Ассеты

- Новые файлы класть в `Assets/`; ID и метаданные создаёт редактор после сканирования.
- Не редактировать вручную служебные реестры, манифесты, cookies, cooked/cache-файлы
  внутри `.Panda/` и sidecar `.pmeta`.
- Документация/скиллы описывают `.Panda/assets.json` как реестр. В текущем пустом
  проекте этот файл ещё не создан, а у `world.world` уже есть sidecar `.pmeta`.
  Перед использованием ID смотреть фактически сгенерированные редактором метаданные;
  если ID отсутствует — просить пользователя сделать Reload/rescan, а не придумывать ID.
- Цветовые текстуры — sRGB. Для normal/roughness/noise и других data maps пользователь
  должен отключить sRGB в инспекторе.
- `.mat`: можно менять значения `inputs`, но нельзя менять набор/порядок полей контракта
  шейдера. Новый материал создавать через редактор.
- glTF хранить вместе с `.bin` и текстурами; созданные редактором материалы не дублировать
  вручную без необходимости.

## Полезные примеры

### ClawnDash — основной референс структуры игры

- `PandaExamples/ClawnDash/Assets/menu.world` — отдельный мир меню.
- `PandaExamples/ClawnDash/Assets/levels/level1.world` и `level2.world` — отдельные
  редактируемые уровни.
- `ClawnDashMenu.{hpp,cpp}` — `WorldHandle` уровней, PandaUI main menu и загрузка через
  `WorldAPI::load()`.
- `ClawnDashLevelController.{hpp,cpp}` — ссылки на menu/next world, поиск заранее
  размещённых объектов через `findByTag/findAllByTag`, HUD и переходы.
- В `shutdown()` UI root очищается. Это важно при смене world и выгрузке скрипта.
- Уровни заранее содержат Player, Ground, Solid, Hazard, Jump Pad/Orb, portals и Finish;
  код лишь читает и управляет ими. Это образец scene-authored подхода.
- Не копировать слепо стартовый `.Panda/project.json` ClawnDash: в текущем примере он
  указывает на level1, хотя отдельный menu world существует.

### Остальные референсы

- `PandaExamples/Platformer`: world-authored 2D-сцена, иерархия, физика, сенсоры,
  переход между `world.world`/`world2.world`; есть готовый
  `.Panda/scripts-manifest.json` для сверки формата script fields.
- `PandaExamples/PhysicsTest`: форматы 2D/3D rigidbody, colliders и joints.
- `PandaExamples/HelloUI`: обзор элементов PandaUI, layout и нескольких окон.
- `PandaExamples/Game2048`: модель полностью динамического UI; полезна для UI-состояния,
  но не как образец редактируемого игрового уровня.
- `PandaExamples/Neverland`: крупный 3D-проект, terrain, runtime mesh/texture, save data,
  сложный UI и async-паттерны.
- `PandaExamples/TerrainTest` и `Sponza`: terrain, материалы, postprocessing и 3D-ассеты.

## Предыдущая неудачная попытка: koiyamich

Путь: `/Users/milan/PandaProjects/koiyamich`. Документация этого проекта устарела;
использовать только текущие `PandaSDK/docs`, заголовки и скиллы `liberate_the_sheep`.

### Не переносить как архитектуру

- В старой попытке меню, настройки, будущая игра и Game Over объединены в одном
  `Assets/world.world`; переходов через `WorldHandle`/`WorldAPI::load()` нет.
- `GameRoot` одновременно отвечает за UI, сохранения, ресурсы, ввод, меню и gameplay.
- Статические декорации частично есть в world, но код дублирует их через fallback
  `WorldAPI::createEntity()`. В новой игре world — единственный источник статической сцены.
- В коде захардкожены asset ID; старые ID и `.pmeta` нельзя переносить в новый проект.
- Овца, куст и забор создаются runtime-мешами; овца превращает каждый непрозрачный
  пиксель PNG в отдельный quad. Для такой 2D-графики использовать Sprite.
- UI-картинки генерируются из raw pixels/больших `.inl`. Эти массивы, runtime-текстуры,
  кеши `.ptex`, ScriptCache, build/bin и старый world не переносить.
- В старом проекте вообще нет `.mat`/шейдеров: все Sprite используют `material: 0`,
  runtime-меши — vertex colors. Поэтому он не является референсом работы с материалами.
- `material: 0` допустим для обычного Sprite со встроенным материалом. Собственный
  `.mat` нужен только для отдельного шейдера/эффекта; создавать и назначать его через
  PandaEditor, затем при необходимости править только значения `inputs`.

### Проверенные ошибки поведения

- Play нарисован как обычный `PandaUI::View`, не имеет callback и не запускает игру.
- Кнопка Settings явно disabled и тоже не вызывает существующий callback.
- В `Runner` условие первого spawn никогда не выполняется, а spawn-coordinate не
  уменьшается; препятствия не появились бы даже после починки Play.
- `orthoSize = 4.5`, а земля/объекты расположены примерно на `y = -6.2`; композиция
  вероятно находится вне видимой области при принятой кодом трактовке камеры.
- Сценовые сущности runner не очищаются при возврате в меню того же world.

### Потенциально переиспользуемые исходники

Копировать только после игрового/визуального брифа, в `liberate_the_sheep/Assets/`
(например, `Assets/Textures/`), без старых `.pmeta`; новые ID назначает редактор.

- `Scripts/koiyamich/src/Game/art/sheep_source.png` — овца 72×72, использовать как Sprite.
- `.../cloud_source.png` — облако 32×32.
- `.../sun_source.png` — солнце 226×222.
- `.../earth_platform_tile.png` — земляной тайл 28×9.
- `.../earth019_sheet.png` — atlas 4×1; анимация через `setCell(cols=4, rows=1, index)`.
- `.../bush_green.png` — куст 399×265.
- `Assets/sky.png` — небо 2284×1224; `art/sky_source.png` — идентичный дубликат,
  переносить только одну копию.

Для pixel-art при импорте проверить nearest/no-mipmap настройки: старые параметры
фильтрации размывают тайлы и могут давать bleed atlas. Происхождение/лицензии графики
не оформлены: часть файлов пришла с OpenGameArt, для овцы указан Twemoji CC-BY 4.0,
а сведения о небе противоречат друг другу. Не использовать в релизе без проверки
лицензии и требуемой атрибуции.

### Идеи, которые можно реализовать заново

- Отдельная menu-world диорама: дрейфующие облака и динамические овцы, которые входят
  с краёв и автоматически перепрыгивают заранее размещённые Sprite-препятствия.
- Sprite-sheet анимация через `SpriteRendererComponentAPI::setCell()`.
- Таблица локализации по enum/key, если будущей игре действительно нужны эти языки.
- Сохранение небольшого прогресса через `ApplicationAPI::getPersistentDataPath()`.
- Обновление существующего HUD Label (`setText`), без пересоздания всего UI каждый кадр.
- Действительно динамических актёров разрешено создавать из кода, но их визуальные
  ресурсы должны быть импортированы через движок, а runtime-сущности — очищаться.

## Текущее состояние liberate_the_sheep

- `Assets/world.world` — стартовый шаблон с Camera, Directional Light, Orange Sprite и Sky.
- `Scripts/liberate_the_sheep/src/SampleScript.*` — демонстрационный скрипт.
- `.Panda/project.json` указывает на `Assets/world.world`.
- Сейчас отсутствуют `.Panda/scripts-manifest.json`, `.Panda/assets.json`, собранная
  библиотека скриптов и cooked runtime data. Перед первым связыванием полей нужна
  сборка + Reload; перед standalone export нужен Export Game из PandaEditor.
- Текущая папка не является Git-репозиторием: изменения нельзя считать автоматически
  защищёнными историей Git.

## Проверенные ограничения текущего SDK

- ABI скриптов: версия 23.
- В `Input.hpp` объявлены `getMousePositionX/Y()` и `getMouseScrollX/Y()`, но в
  поставленной `libPandaSDK.a` для них нет символов. Пока не использовать: будет ошибка
  линковки. `getMouseDeltaX/Y()`, клавиши, кнопки мыши и touch доступны.
- Установленный пакет содержит библиотеки только для macOS/Metal; экспорт на другие
  платформы потребует соответствующего PandaSDK.
