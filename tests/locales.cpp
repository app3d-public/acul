#include <acul/locales/locales.hpp>
#include <acul/string/string.hpp>
#include <cassert>

using namespace acul;
using namespace acul::locales;

void test_locales()
{
    const char *lang = "en";
    string l = get_user_language(&lang, 1);
    assert(!l.empty());

    setup_i18n(l);
}
