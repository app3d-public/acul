#ifndef APP_CORE_STD_ENUM_H
#define APP_CORE_STD_ENUM_H

#include <type_traits>

template <typename BitType>
struct FlagTraits
{
    static constexpr bool isBitmask = false; // По умолчанию false
};

template <typename BitType>
class Flags
{
public:
    using MaskType = typename std::underlying_type<BitType>::type;

    constexpr Flags() noexcept : _mask(0) {}
    constexpr Flags(BitType bit) noexcept : _mask(static_cast<MaskType>(bit)) {}
    constexpr explicit Flags(MaskType flags) noexcept : _mask(flags) {}

    constexpr Flags operator&(Flags const &rhs) const noexcept { return Flags(_mask & rhs._mask); }

    constexpr Flags operator|(Flags const &rhs) const noexcept { return Flags(_mask | rhs._mask); }

    constexpr Flags operator^(Flags const &rhs) const noexcept { return Flags(_mask ^ rhs._mask); }

    constexpr Flags operator~() const noexcept { return Flags(_mask ^ FlagTraits<BitType>::allFlags._mask); }

    constexpr Flags &operator|=(Flags const &rhs) noexcept
    {
        _mask |= rhs._mask;
        return *this;
    }

    constexpr Flags &operator&=(Flags const &rhs) noexcept
    {
        _mask &= rhs._mask;
        return *this;
    }

    constexpr Flags &operator^=(Flags const &rhs) noexcept
    {
        _mask ^= rhs._mask;
        return *this;
    }

    constexpr operator bool() const noexcept { return _mask != 0; }

    constexpr operator MaskType() const noexcept { return _mask; }

    friend constexpr bool operator==(const Flags &lhs, const Flags &rhs) noexcept { return lhs._mask == rhs._mask; }

    constexpr MaskType getMask() const noexcept { return _mask; }

private:
    MaskType _mask;
};

template <typename BitType>
constexpr Flags<BitType> operator&(BitType bit, Flags<BitType> const &flags) noexcept
{
    return flags.operator&(bit);
}

template <typename BitType>
constexpr Flags<BitType> operator|(BitType bit, Flags<BitType> const &flags) noexcept
{
    return flags.operator|(bit);
}

template <typename BitType>
constexpr Flags<BitType> operator^(BitType bit, Flags<BitType> const &flags) noexcept
{
    return flags.operator^(bit);
}

// bitwise operators on BitType
template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator&(BitType lhs, BitType rhs) noexcept
{
    return Flags<BitType>(lhs) & rhs;
}

template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator|(BitType lhs, BitType rhs) noexcept
{
    return Flags<BitType>(lhs) | rhs;
}

template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator^(BitType lhs, BitType rhs) noexcept
{
    return Flags<BitType>(lhs) ^ rhs;
}

template <typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator~(BitType bit) noexcept
{
    return ~(Flags<BitType>(bit));
}

template <typename BitType>
inline constexpr bool operator&(Flags<BitType> const &flags, BitType bit) noexcept
{
    using MaskType = typename Flags<BitType>::MaskType;
    return (static_cast<MaskType>(flags) & static_cast<MaskType>(bit)) != 0;
}

#endif