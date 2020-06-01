// implementation details for philox and counter_based_engine:
//
//   integral_innput-range - concepts
//   uint_fast<w> - the fast unsigned type with at least w bits
//   uint_least<w> - the smallest unsigned type with at least w bits
//   fffmask<Uint, w> - the Uint with the low w bits set
//   mulhilo<w, Uint> -> pair<U, U> - returns the w hi
//       and w low bits of the 2w-bit product of a and b.

#pragma once
#include <concepts>
#include <iterator>

namespace std{
namespace detail{

template <typename X>
concept integral_input_range =
    ranges::input_range<X> &&
    integral<ranges::range_value_t<X>>;

// uint_fast<w> :  the fast unsigned integer type with at least w bits.
// uint_least<w> : the smallest unsigned integer type with at least w bits.
// N.B.  This is a whole lot simpler than it used to be!
constexpr unsigned next_stdint(unsigned w){
    if (w<=8) return 8;
    else if (w<=16) return 16;
    else if (w<=32) return 32;
    else if (w<=64) return 64;
    else if (w<=128) return 128; // DANGER - assuming __uint128_t
    return 0;
}

template<unsigned w>
struct ui{};
template<> struct ui<8>{ using least = uint_least8_t; using fast= uint_fast8_t; };
template<> struct ui<16>{ using least = uint_least16_t; using fast= uint_fast16_t; };
template<> struct ui<32>{ using least = uint_least32_t; using fast= uint_fast32_t; };
template<> struct ui<64>{ using least = uint_least64_t; using fast= uint_fast64_t; };
template<> struct ui<128>{ using least = __uint128_t; using fast= __uint128_t; }; // DANGER - __uint128_t

template <unsigned w>
using uint_least = ui<next_stdint(w)>::least;
template <unsigned w>
using uint_fast = ui<next_stdint(w)>::fast;

// Implement w-bit mulhilo with an 2w-wide integer.  If we don't
// have a 2w-wide integer, we're out of luck.
template <unsigned  w, unsigned_integral U>
static pair<U, U> mulhilo(U a, U b){
    using uwide = uint_fast<2*w>;
    const size_t xwidth = numeric_limits<uwide>::digits;
    uwide ab = uwide(a) * uwide(b);
    return {U(ab>>w), U((ab<<(xwidth-w)) >> (xwidth-w))};
}

template <std::unsigned_integral U, unsigned w>
requires (w <= std::numeric_limits<U>::digits)
constexpr U fffmask = w ? (U(~(U(0))) >> (std::numeric_limits<U>::digits - w)) : 0;

} // namespace detail
} // namespace std
