---
name: panda-build-scripts
description: Сборка и перезагрузка C++-скриптов игры (Scripts/). Используй после правки кода скриптов.
---

# Сборка скриптов

Скрипты — C++ в `Scripts/liberate_the_sheep/src/`, собираются в динамическую
библиотеку, которую редактор загружает.

## Цикл

1. Правишь код (скелет скрипта и поля — PandaSDK `docs/scripting.md`:
   `Script`-класс, `PANDA_FIELDS_*`, `REGISTER_SCRIPT`).
2. Сборка из терминала:
   `Scripts/liberate_the_sheep/scripts/configure_scripts.sh debug` (однократно),
   затем `Scripts/liberate_the_sheep/scripts/build_scripts.sh debug`.
   Ошибки компиляции чини сам по выводу.
3. Попроси пользователя нажать Reload (Cmd/Ctrl+R) — редактор подхватит свежую
   библиотеку (по mtime). Он же запускает и проверяет игру.

После загрузки скриптов редактор обновляет `.Panda/scripts-manifest.json` —
оттуда бери поля для привязки в мире (panda-script-fields).
