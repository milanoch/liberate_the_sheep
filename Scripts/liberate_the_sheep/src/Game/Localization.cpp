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

        {"ВЫБЕРИ ПОЛЕ", "CHOOSE A FIELD"},
        {"Три размера поля, девять головоломок.",
         "Three field sizes, nine deterministic puzzles."},
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
