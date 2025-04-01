#include <acul/locales.hpp>
#include <algorithm>
#include <array>
#include <clocale>
#ifdef _WIN32
    #include <winnls.h>
#endif

namespace acul
{
    namespace locales
    {
        void setup_i18n(const string &locale)
        {
#ifdef _WIN32
            wstring wlocale = wstring(locale.begin(), locale.end());
            _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
            const auto localeId = LocaleNameToLCID(wlocale.c_str(), LOCALE_ALLOW_NEUTRAL_NAMES);
            SetThreadLocale(localeId);
#else
            setlocale(LC_MESSAGES, locale.data());
#endif
        }

        string get_user_language()
        {
            constexpr std::array<string_view, 2> supported_languages = {"en", "ru"};
            string shell_locale = setlocale(LC_MESSAGES, "");
            size_t underscore_pos = shell_locale.find("_");
            string short_locale;
            if (underscore_pos != SIZE_MAX) short_locale = shell_locale.substr(0, underscore_pos);
            if (std::count(supported_languages.begin(), supported_languages.end(), short_locale))
                return shell_locale;
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
} // namespace acul