#ifndef APP_ABC_HPP
#define APP_ABC_HPP

#include <libintl.h>
#include <string>
#include "api.hpp"

#define _(STRING) gettext(STRING)

namespace locales
{
    APPLIB_API void setup_i18n(const std::string &locale);
    
    APPLIB_API std::string getUserLanguage();
} // namespace locales

#endif