#ifndef APP_CORE_STD_ENUM_H
#define APP_CORE_STD_ENUM_H

#include <type_traits>

namespace acul
{
    template <typename T, typename = void>
    struct has_flag_bitmask : std::false_type
    {
    };

    template <typename T>
    struct has_flag_bitmask<T, std::void_t<typename T::flag_bitmask>> : std::true_type
    {
    };

    template <typename BitType>
    using is_flags = std::enable_if_t<has_flag_bitmask<BitType>::value>;

    template <typename BitType>
    constexpr std::underlying_type_t<typename BitType::enum_type> compute_all_flags() noexcept
    {
        using mask_t = std::underlying_type_t<typename BitType::enum_type>;
        mask_t all = 0;
        for (mask_t i = 0; i < sizeof(mask_t) * 8; ++i) all |= (mask_t{1} << i);
        return all;
    }

    template <typename BitType, typename = is_flags<BitType>>
    struct flag_traits
    {
        using enum_type = typename BitType::enum_type;
        using mask_t = std::underlying_type_t<enum_type>;

        static constexpr enum_type all_flags = static_cast<enum_type>(compute_all_flags<BitType>());
    };

    template <typename BitType, typename = std::enable_if_t<has_flag_bitmask<BitType>::value>>
    class flags
    {
    public:
        using enum_type = typename BitType::enum_type;
        using mask_t = typename std::underlying_type_t<enum_type>;

        constexpr flags() noexcept : _mask(0) {}
        constexpr flags(BitType bit) noexcept : _mask(static_cast<mask_t>(bit)) {}
        constexpr flags(mask_t value) noexcept : _mask(value) {}

        template <typename T>
        constexpr std::enable_if_t<has_flag_bitmask<T>::value, flags> operator&(flags const &rhs) const noexcept
        {
            return flags(_mask & rhs._mask);
        }

        template <typename T>
        constexpr std::enable_if_t<has_flag_bitmask<T>::value, flags> operator|(flags const &rhs) const noexcept
        {
            return flags(_mask & rhs._mask);
        }

        constexpr flags operator^(flags const &rhs) const noexcept { return flags(_mask ^ rhs._mask); }
        constexpr flags operator~() const noexcept
        {
            return flags(static_cast<mask_t>(flag_traits<BitType>::all_flags) ^ _mask);
        }

        constexpr flags &operator|=(flags const &rhs) noexcept
        {
            _mask |= rhs._mask;
            return *this;
        }
        constexpr flags &operator&=(flags const &rhs) noexcept
        {
            _mask &= rhs._mask;
            return *this;
        }
        constexpr flags &operator^=(flags const &rhs) noexcept
        {
            _mask ^= rhs._mask;
            return *this;
        }

        constexpr operator mask_t() const noexcept { return _mask; }
    public:
        mask_t _mask;
    };

} // namespace acul

template <typename BitType, typename = acul::is_flags<BitType>>
constexpr acul::flags<BitType> operator&(BitType bit, acul::flags<BitType> const &flags) noexcept
{
    return flags.operator&(bit);
}

template <typename BitType, typename = acul::is_flags<BitType>>
constexpr acul::flags<BitType> operator|(BitType bit, acul::flags<BitType> const &flags) noexcept
{
    return flags.operator|(bit);
}

template <typename BitType, typename = acul::is_flags<BitType>>
constexpr acul::flags<BitType> operator^(BitType bit, acul::flags<BitType> const &flags) noexcept
{
    return flags.operator^(bit);
}

// bitwise operators on BitType
template <typename BitType, typename = acul::is_flags<BitType>>
inline constexpr acul::flags<BitType> operator&(BitType lhs, BitType rhs) noexcept
{
    return acul::flags<BitType>(lhs) & rhs;
}

template <typename BitType, typename = acul::is_flags<BitType>>
inline constexpr acul::flags<BitType> operator|(BitType lhs, BitType rhs) noexcept
{
    return acul::flags<BitType>(lhs) | rhs;
}

template <typename BitType, typename = acul::is_flags<BitType>>
inline constexpr acul::flags<BitType> operator^(BitType lhs, BitType rhs) noexcept
{
    return acul::flags<BitType>(lhs) ^ rhs;
}

template <typename BitType, typename = acul::is_flags<BitType>>
inline constexpr acul::flags<BitType> operator~(BitType bit) noexcept
{
    return ~(acul::flags<BitType>(bit));
}

template <typename BitType, typename = acul::is_flags<BitType>>
inline constexpr bool operator&(acul::flags<BitType> const &flags, BitType bit) noexcept
{
    using mask_t = typename acul::flags<BitType>::mask_t;
    return (static_cast<mask_t>(flags) & static_cast<mask_t>(bit)) != 0;
}

#endif
