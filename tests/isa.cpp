#include <acul/ipc.hpp>
#include <acul/isa.hpp>
#include <cassert>
#include <cstddef>

void test_isa()
{
    const bool supported = acul::is_x86_64_v3_supported();
#if defined(__x86_64__) || defined(_M_X64)
    (void)supported;
#else
    assert(!supported);
#endif

    static_assert(std::is_standard_layout_v<acul::crash_notify>);
    static_assert(sizeof(acul::crash_notify) == 24);
    static_assert(offsetof(acul::crash_notify, addr) == 16);

    acul::crash_notify pkt{};
    assert(pkt.magic == ACUL_CRASH_NOTIFY_MAGIC);
}
