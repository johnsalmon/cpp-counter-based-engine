#pragma once

#include "detail.hpp" // a couple of helpful functions and concepts
#include <array>
#include <ranges>

namespace std{

template<size_t w, size_t n, size_t r, typename detail::uint_fast<w> ... consts>
class philox_prf{
    static_assert(w>0);
    static_assert(n==2 || n==4); // FIXME - 8, 16?
    static_assert(sizeof ...(consts) == n);
    // assert that all constants are < 2^w ?
    using Uint = detail::uint_fast<w>;

public:
    static constexpr size_t in_bits = w;
    static constexpr size_t in_N = 3*(n/2);
    static constexpr size_t result_bits = w;
    static constexpr size_t result_N = n;
    using in_value_type = Uint;
    using result_value_type = Uint;
    using result_type = array<result_value_type, result_N>;

    template <integral T>
    result_type operator()(initializer_list<T> il) const {
        return operator()(ranges::subrange(il));
    }

    template <detail::integral_input_range InRange>
    result_type operator()(InRange in) const {
        static_assert(is_integral_v<ranges::range_value_t<InRange>>);
        auto cp = ranges::begin(in);
        auto ce = ranges::end(in);
        if constexpr (n == 2){
            in_value_type K0 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            in_value_type R0 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            in_value_type L0 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            for(size_t i=0; i<r; ++i){
                auto [hi, lo] = detail::mulhilo<w>(R0, MC[0]);
                R0 = hi^K0^L0;
                L0 = lo;
                K0 = (K0+MC[1]) & inmask;
            }
            return {R0, L0};
        }else if constexpr (n == 4) {
            in_value_type K0 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            in_value_type K1 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            in_value_type R0 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            in_value_type L0 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            in_value_type R1 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            in_value_type L1 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            for(size_t i=0; i<r; ++i){
                auto [hi0, lo0] = detail::mulhilo<w>(R0, MC[0]);
                auto [hi1, lo1] = detail::mulhilo<w>(R1, MC[2]);
                R0 = hi1^L0^K0;
                L0 = lo1;
                R1 = hi0^L1^K1;
                L1 = lo0;
                K0 = (K0 + MC[1]) & inmask;
                K1 = (K1 + MC[3]) & inmask;
            }
            return {R0, L0, R1, L1};
        }
        // No more cases.  See the static_assert(n==2 || n==4) at the top of the class
    }

private:
    static constexpr array<Uint, n> MC = {consts...};
    static constexpr in_value_type inmask = detail::fffmask<in_value_type, in_bits>;
}; 

// FIXME - the template alias lets the program choose a different value of R (good)
// but it also forces the programmer to add an empty template parameter list if
// the default value is desired (bad).  Some additional syntactic sugar is needed.
template <unsigned R=10>
using philox2x32_prf = philox_prf<32, 2, R, 0xD256D193, 0x9E3779B9>;
template<unsigned R=10>
using philox4x32_prf = philox_prf<32, 4, R, 0xD2511F53, 0x9E3779B9,
                                                    0xCD9E8D57, 0xBB67AE85>;

template <unsigned R=10>
using philox2x64_prf = philox_prf<64, 2, R, 0xD2B74407B1CE6E93, 0x9E3779B97F4A7C15>;
template <unsigned R=10>
using philox4x64_prf = philox_prf<64, 4, R, 0xD2E7470EE14C6C93, 0x9E3779B97F4A7C15,
                                                    0xCA5A826395121157, 0xBB67AE8584CAA73B>;
} // namespace std

#include "counter_based_engine.hpp"
namespace std{

using philox2x32 = counter_based_engine<philox2x32_prf<>>;
using philox4x32 = counter_based_engine<philox4x32_prf<>>;
using philox2x64 = counter_based_engine<philox2x64_prf<>>;
using philox4x64 = counter_based_engine<philox4x64_prf<>>;

} // namespace std
