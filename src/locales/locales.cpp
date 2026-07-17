#include <acul/locales/locales.hpp>
#include <acul/string/string.hpp>
#include <clocale>
#include <cstdlib>
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
            // GNU gettext uses the C runtime/process environment rather than the Win32 thread locale.
            // Explicitly select the configured catalog, even if the process was started with LC_ALL=C.
            _putenv_s("LANGUAGE", locale.c_str());
            _putenv_s("LC_ALL", locale.c_str());
            _putenv_s("LC_MESSAGES", locale.c_str());
#else
            setlocale(LC_MESSAGES, locale.c_str());
#endif
        }

        string get_user_language(const char **pLanguages, size_t count)
        {
            auto find_supported = [pLanguages, count](const string &language) -> string {
                for (size_t i = 0; i < count; ++i)
                    if (language == pLanguages[i]) return pLanguages[i];
                return {};
            };

#ifdef _WIN32
            wchar_t locale_name[LOCALE_NAME_MAX_LENGTH]{};
            wchar_t language_name[LOCALE_NAME_MAX_LENGTH]{};
            if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0 &&
                GetLocaleInfoEx(locale_name, LOCALE_SISO639LANGNAME, language_name, LOCALE_NAME_MAX_LENGTH) > 0)
            {
                string language;
                for (const wchar_t *it = language_name; *it != L'\0'; ++it)
                    language.push_back(static_cast<char>(*it));
                if (auto supported = find_supported(language); !supported.empty()) return supported;
            }
#endif

            const char *resolved_locale = setlocale(LC_MESSAGES, "");
            string shell_locale = resolved_locale ? resolved_locale : "";
            size_t language_end = shell_locale.size();
            for (const char *separator : {"_", ".", "-"})
            {
                const size_t position = shell_locale.find(separator);
                if (position != SIZE_MAX && position < language_end) language_end = position;
            }
            const string short_locale = shell_locale.substr(0, language_end);
            if (auto supported = find_supported(short_locale); !supported.empty()) return supported;
            return count > 0 ? pLanguages[0] : string{};
        }
    } // namespace locales
} // namespace acul
