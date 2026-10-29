#include "Game/Localization.hpp"

#include <array>

namespace LiberateTheSheep::Game {
namespace {

    struct Translation final {
        std::string_view russian;
        std::string_view english;
    };

    constexpr std::array<Translation, static_cast<std::size_t>(TextKey::Count)> kTranslations{{
        {"ДЕМО PANDA ENGINE", "PANDA ENGINE DEMO"},
        {"ОСВОБОДИ\nОВЕЧЕК", "LIBERATE\nTHE SHEEP"},
        {"Найди свободный путь и помоги каждой овечке выбраться из стада.",
         "Find a clear path and help every two-cell sheep escape the flock."},
        {"ПРОДОЛЖИТЬ", "CONTINUE"},
        {"ИГРАТЬ", "PLAY"},
        {"ВЫБРАТЬ УРОВЕНЬ", "LEVEL SELECT"},
        {"Нажимай на овечку, только когда путь перед ней свободен.",
         "Tap a sheep only when the way ahead is clear."},

        {"УРОВНИ", "LEVELS"},
        {"39 уровней. Освободи всё стадо!",
         "39 levels. Set the whole flock free!"},
        {"УРОВЕНЬ", "LEVEL"},
        {"ЗАКРЫТ", "LOCKED"},
        {"ГОТОВ", "READY"},
        {"СЕРДЦА", "HEARTS"},
        {"НАЗАД", "BACK"},

        {"НАСТРОЙКИ", "SETTINGS"},
        {"НАСТРОЙКИ", "SETTINGS"},
        {"ЗАКРЫТЬ", "CLOSE"},
        {"ЯЗЫК", "LANGUAGE"},
        {"РУССКИЙ", "RUSSIAN"},
        {"АНГЛИЙСКИЙ", "ENGLISH"},
        {"МУЗЫКА", "MUSIC"},
        {"ВКЛ.", "ON"},
        {"ВЫКЛ.", "OFF"},
        {"СБРОСИТЬ ПРОГРЕСС", "RESET PROGRESS"},
        {"Все открытые уровни и рекорды будут удалены.",
         "All unlocked levels and records will be removed."},
        {"Сбросить весь прогресс?", "Reset all progress?"},
        {"ОТМЕНА", "CANCEL"},
        {"СБРОСИТЬ", "RESET"},
        {"ПРОГРЕСС СБРОШЕН", "PROGRESS RESET"},

        {"УРОВЕНЬ", "LEVEL"},
        {"СЕРДЦА", "HEARTS"},
        {"ОСТАЛОСЬ ОВЕЧЕК", "SHEEP LEFT"},
        {"ПОДСКАЗКА", "HINT"},
        {"ЗАНОВО", "RESTART"},
        {"МЕНЮ", "MENU"},

        {"ПОЛЕ ОЧИЩЕНО", "FIELD CLEARED"},
        {"ОВЕЧКИ СВОБОДНЫ!", "THE FLOCK IS FREE!"},
        {"Ты очистил поле.", "You cleared the field."},
        {"СЕРДЦА ЗАКОНЧИЛИСЬ", "NO HEARTS LEFT"},
        {"ОВЕЧКАМ НУЖНА ПОМОЩЬ", "THE SHEEP NEED YOU"},
        {"Попробуй ещё раз и сначала найди свободный путь.",
         "Try the same field again and look for a clear lane first."},
        {"СЛЕДУЮЩИЙ УРОВЕНЬ", "NEXT LEVEL"},
        {"ПЕРЕИГРАТЬ", "REPLAY"},

        {"БЕССМЕРТНЫЙ РЕЖИМ", "IMMORTAL MODE"},
        {"Без потери жизней и поражений от бомбы. Счётчик сердец скрыт.",
         "No lost lives or bomb defeats. The heart counter is hidden."},

        {"ОВЕЧКА С БОМБОЙ", "A SHEEP WITH A BOMB"},
        {"На спине этой овечки — красная бомба. Освободи ей путь: овечка должна целиком выйти через ворота до нуля на таймере. В обычном режиме взрыв означает поражение. Отсчёт начнётся после нажатия кнопки ниже.",
         "This sheep carries a red bomb. Clear its path: the whole sheep must pass through the gate before the timer reaches zero. In normal mode an explosion means defeat. The countdown starts when you press the button below."},
        {"В бессмертном режиме таймер учебный: на нуле бомба безопасно погаснет. Овечка останется в игре, и ты сможешь продолжить без поражения.",
         "In immortal mode the timer is for practice: at zero the bomb safely fizzles out. The sheep stays in play and you can continue without losing."},
        {"ПОЕХАЛИ!", "LET'S GO!"},
        {"ДО ВЗРЫВА", "BOMB TIMER"},
        {"ОВЕЧКА В БЕЗОПАСНОСТИ", "SHEEP RESCUED"},
        {"БОМБА ПОГАШЕНА", "BOMB DEFUSED"},
        {"ВРЕМЯ ВЫШЛО", "TIME IS UP"},
        {"Овечка с бомбой не успела выйти из загона. Попробуй ещё раз: сначала освободи ей путь к воротам.",
         "The sheep with the bomb did not leave the pen in time. Try again and clear its route to the gate first."},

        {"ОВЕЧКА С ЧЁРНОЙ БОМБОЙ", "A SHEEP WITH A BLACK BOMB"},
        {"Число на чёрной бомбе — сколько ударов осталось до взрыва. Оно уменьшается на один, когда другая овечка упирается прямо в эту овечку. Например, при числе 3 третий удар вызовет взрыв и поражение. Удар по другой овечке не считается. Такие удары расходуют заряд бомбы вместо сердца. Освободи путь и выведи овечку целиком через ворота, чтобы обезвредить бомбу.",
         "The number on the black bomb shows how many hits remain before it explodes. It drops by one when another sheep runs directly into this sheep. With a 3, the third hit causes an explosion and defeat. Hitting a different sheep does not count. These hits use a bomb charge instead of a heart. Clear the way and get the whole sheep through the gate to defuse the bomb."},
        {"В бессмертном режиме число тоже уменьшается от ударов, но на нуле бомба безопасно погаснет. Овечка останется в игре, и ты сможешь продолжить без поражения.",
         "In immortal mode hits still reduce the number, but at zero the bomb safely fizzles out. The sheep stays in play and you can continue without losing."},
        {"УДАРОВ ДО ВЗРЫВА", "HITS UNTIL EXPLOSION"},
        {"БОМБА НЕ ВЫДЕРЖАЛА УДАР", "THE BOMB WAS HIT TOO OFTEN"},
        {"В овечку с чёрной бомбой врезались слишком много раз. Попробуй ещё раз и освободи ей путь, не направляя на неё других овечек.",
         "The sheep with the black bomb took too many hits. Try again and clear its route without sending other sheep into it."},
        {"ЧИТ: ВСЕ УРОВНИ", "CHEAT: ALL LEVELS"},
        {"Открывает все уровни для проверки. Сохранённый прогресс не меняется.",
         "Open every level for testing. Your saved progress stays unchanged."},
    }};

    static_assert(kTranslations.size() == static_cast<std::size_t>(TextKey::Count));

    [[nodiscard]] const Translation &translation(TextKey key) noexcept {
        const auto index = static_cast<std::size_t>(key);
        if (index >= kTranslations.size()) {
            return kTranslations[static_cast<std::size_t>(TextKey::GameTitle)];
        }
        return kTranslations[index];
    }

} // namespace

std::string_view languageCode(Language language) noexcept {
    return language == Language::English ? "en" : "ru";
}

std::string_view localizedText(TextKey key, Language language) noexcept {
    const Translation &value = translation(key);
    return language == Language::English ? value.english : value.russian;
}

std::string_view localizedLanguageName(Language language, Language displayLanguage) noexcept {
    return localizedText(
        language == Language::English ? TextKey::English : TextKey::Russian,
        displayLanguage
    );
}

} // namespace LiberateTheSheep::Game
