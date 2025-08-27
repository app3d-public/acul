#include <acul/locales/locales.hpp>
#include <acul/string/string.hpp>
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
            setlocale(LC_MESSAGES, locale.c_str());
#endif
        }

        string get_user_language(const char **pLanguages, size_t count)
        {
            string shell_locale = setlocale(LC_MESSAGES, "");
            size_t underscore_pos = shell_locale.find("_");
            string short_locale;
            if (underscore_pos != SIZE_MAX) short_locale = shell_locale.substr(0, underscore_pos);
            for (size_t i = 0; i < count; ++i)
                if (short_locale == pLanguages[i]) return shell_locale;
#ifdef _WIN32
            return "en_US";
#else
            return "en_US.UTF-8";
#endif
        }
    } // namespace locales
} // namespace acul