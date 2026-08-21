#include <acul/isa.hpp>
#include <cassert>

void test_isa()
{
    const bool supported = acul::is_x86_64_v3_supported();
#if defined(__x86_64__) || defined(_M_X64)
    (void)supported;
#else
    assert(!supported);
#endif
}
