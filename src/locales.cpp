#include <algorithm>
#include <clocale>
#include <core/locales.hpp>
#include <core/std/basic_types.hpp>
#ifdef _WIN32
    #include <winnls.h>
#endif

namespace locales
{
    void setup_i18n(const std::string &locale)
    {
#ifdef _WIN32
        std::wstring wlocale = std::wstring(locale.begin(), locale.end());
        _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
        const auto localeId = LocaleNameToLCID(wlocale.c_str(), LOCALE_ALLOW_NEUTRAL_NAMES);
        SetThreadLocale(localeId);
#else
        setlocale(LC_MESSAGES, locale.data());
#endif
    }

    std::string getUserLanguage()
    {
        constexpr std::array<std::string, 2> supportedLanguages = {"en", "ru"};
        std::string shellLocale = setlocale(LC_MESSAGES, "");
        size_t underscorePos = shellLocale.find("_");
        std::string shortLocale;
        if (underscorePos != std::string::npos) shortLocale = shellLocale.substr(0, underscorePos);
        if (std::count(supportedLanguages.begin(), supportedLanguages.end(), shortLocale))
            return shellLocale;
        else
        {
#ifdef _WIN32
            return "en_US";
#else
            return "en_US.UTF-8";
#endif
        }
    }
} // namespace locales