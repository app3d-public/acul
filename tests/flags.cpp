#include <acul/enum.hpp>
#include <acul/scalars.hpp>
#include <cassert>

struct FlagBits
{
    enum enum_type : u32
    {
        A = 1 << 0,
        B = 1 << 1,
        C = 1 << 2,
    };
    using flag_bitmask = std::true_type;
};

using Flags = acul::flags<FlagBits>;

void test_flags()
{
    // All flags
    auto all_flags = acul::compute_all_flags<FlagBits>();
    auto expected_mask = static_cast<Flags::mask_t>(-1);
    assert(all_flags == expected_mask);

    // Construct
    Flags f0;
    assert(static_cast<Flags::mask_t>(f0) == 0);

    Flags f1(FlagBits::A);
    assert(static_cast<Flags::mask_t>(f1) == static_cast<Flags::mask_t>(FlagBits::A));

    Flags f2(static_cast<Flags::mask_t>(FlagBits::B));
    assert(static_cast<Flags::mask_t>(f2) == static_cast<Flags::mask_t>(FlagBits::B));

    // Bit op
    Flags combined = f1 | f2;
    assert(static_cast<Flags::mask_t>(combined) ==
           (static_cast<Flags::mask_t>(FlagBits::A) | static_cast<Flags::mask_t>(FlagBits::B)));

    Flags masked = combined & f1;
    assert(static_cast<Flags::mask_t>(masked) == static_cast<Flags::mask_t>(FlagBits::A));

    Flags flipped = ~combined;
    assert((static_cast<Flags::mask_t>(flipped) & static_cast<Flags::mask_t>(FlagBits::A)) == 0);
    assert((static_cast<Flags::mask_t>(flipped) & static_cast<Flags::mask_t>(FlagBits::C)) != 0);

    // Assign
    Flags assign = f1;
    assign |= f2;
    assert(static_cast<Flags::mask_t>(assign) == static_cast<Flags::mask_t>(combined));

    assign &= f1;
    assert(static_cast<Flags::mask_t>(assign) == static_cast<Flags::mask_t>(f1));

    assign ^= f2;
    assert(static_cast<Flags::mask_t>(assign) == (static_cast<Flags::mask_t>(f1) ^ static_cast<Flags::mask_t>(f2)));

    // Global operators
    auto and_result = (FlagBits::A & FlagBits::B);
    assert(static_cast<Flags::mask_t>(and_result) == 0);

    auto or_result = (FlagBits::A | FlagBits::C);
    assert(static_cast<Flags::mask_t>(or_result) ==
           (static_cast<Flags::mask_t>(FlagBits::A) | static_cast<Flags::mask_t>(FlagBits::C)));

    auto xor_result = (FlagBits::A ^ FlagBits::B);
    assert(static_cast<Flags::mask_t>(xor_result) ==
           (static_cast<Flags::mask_t>(FlagBits::A) ^ static_cast<Flags::mask_t>(FlagBits::B)));

    auto not_result = ~(FlagBits::C);
    assert((static_cast<Flags::mask_t>(not_result) & static_cast<Flags::mask_t>(FlagBits::C)) == 0);

    // Check
    Flags fcheck(FlagBits::A);
    assert(fcheck & FlagBits::A);
    assert(!(fcheck & FlagBits::B));
}