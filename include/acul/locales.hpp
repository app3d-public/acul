#ifndef APP_ABC_HPP
#define APP_ABC_HPP

#include <libintl.h>
#include "api.hpp"
#include "fwd/string.hpp"

#define _(STRING) gettext(STRING)

namespace acul
{
    namespace locales
    {
        APPLIB_API void setup_i18n(const string &locale);

        APPLIB_API string get_user_language(const char **pLanguages, size_t count);
    } // namespace locales
} // namespace acul

#endif