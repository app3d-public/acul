#ifndef APP_ABC_HPP
#define APP_ABC_HPP

#include <acul/symbol_export.h>
#include <libintl.h>
#include "../fwd/string.hpp"


#define _(STRING) gettext(STRING)

namespace acul::locales
{
    ACUL_EXPORT void setup_i18n(const string &locale);
    ACUL_EXPORT string get_user_language(const char **pLanguages, size_t count);
} // namespace acul::locales

#endif