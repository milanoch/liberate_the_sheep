---
name: panda-script-fields
description: Привязка значений полей C++-скриптов к сущностям мира (scriptListComponent, fieldId, типы). Используй, когда нужно повесить скрипт на сущность или задать его параметры.
---

# Поля скриптов в мире

Скрипт вешается на сущность в `scriptListComponent`; значения полей задаются там же.

## Источник правды — манифест

`.Panda/scripts-manifest.json` — пишется редактором при каждой загрузке скриптов:
имена классов, поля, их `fieldId` и типы. **Всегда бери fieldId оттуда**, не
вычисляй. Нет файла — попроси пользователя собрать/перезагрузить скрипты.

## Формат в .world

```json
"scriptListComponent": {
    "scripts": [
        {
            "name": "MoveScript",
            "scriptFields": [
                { "name": "speed", "fieldId": 123456789, "type": 1, "value": 3.5 }
            ]
        }
    ]
}
```

- `name` — имя C++-класса (`REGISTER_SCRIPT`).
- `type` числом: 0 integer, 1 float, 2 entity, 3 texture, 4 material, 5 world
  (в манифесте — те же типы словами).
- `value`: integer/float — число; entity — id сущности ЭТОГО мира; texture/
  material/world — asset id из `.Panda/assets.json`.
- Перечисляй только поля, которым нужно значение, отличное от дефолта в коде.
- Скрипт без параметров: `{ "name": "MyScript", "scriptFields": [] }`.

После правки — попроси пользователя нажать Reload (Cmd/Ctrl+R).
