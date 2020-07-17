#pragma once

#include "detail.hpp" // a couple of helpful functions and concepts
#include <array>
#include <ranges>

namespace std{

template<typename UIntType, size_t w, size_t n, size_t r, UIntType ... consts>
class philox_prf{
    static_assert(w>0);
    static_assert(n==2 || n==4); // FIXME - 8, 16?
    static_assert(sizeof ...(consts) == n);
    // assert that all constants are < 2^w ?

public:
    // Differences from P2075R1:
    //   output_value_type and input_value_type instead of result_type
    //   output_word_size and input_word_size instead of word_size
    
    using output_value_type = UIntType; // called result_type in P2075R1
    using input_value_type = UIntType;  // called result_type in P2075R1
    static constexpr size_t input_word_size = w; // called word_size in P2075R1
    static constexpr size_t output_word_size = w;// called word_size in P2075R1
    static constexpr size_t input_count = 3*n/2;
    static constexpr size_t output_count = n;

    // In P2075R1 this returns void, but it makes more sense to return
    // the "final" OutputIterator2
    template<typename InputIterator1, typename OutputIterator2>
    OutputIterator2 operator()(InputIterator1 input, OutputIterator2 output){
        if constexpr (n==2){
            // N.B.  initializers are  evaluated in order, left-to-right.
            array<input_value_type, input_count> inarray = {*input++, *input++, *input++};
            return generate(ranges::single_view(inarray), output);
        }else if constexpr (n==4){
            array<input_value_type, input_count> inarray = {*input++, *input++, *input++, *input++, *input++, *input++};
            return generate(ranges::single_view(inarray), output);
        }            
    }

    // This is a candidate for the "iterator-based API" for bulk generation.  (Not in P2075R1)
    template <typename InRange, weakly_incrementable O>
    O generate(InRange&& inrange, O result) const {
        // FIXME - InRange should be constrained to be a range of
        // array-like objects.  InRange's value_type must itself have
        // a value_type, and it must be indexable with square braces
        // (though we could probably relax that).
        static_assert(is_integral_v<typename ranges::range_value_t<InRange>::value_type>);
        for(const auto& in : inrange){
            if constexpr (n == 2){
                input_value_type R0 = in[0] & inmask;
                input_value_type L0 = in[1] & inmask;
                input_value_type K0 = in[2] & inmask;
                for(size_t i=0; i<r; ++i){
                    auto [hi, lo] = detail::mulhilo<w>(R0, MC[0]);
                    R0 = hi^K0^L0;
                    L0 = lo;
                    K0 = (K0+MC[1]) & inmask;
                }
                *result++ = R0;
                *result++ = L0;
            }else if constexpr (n == 4) {
                input_value_type R0 = in[0] & inmask;
                input_value_type L0 = in[1] & inmask;
                input_value_type R1 = in[2] & inmask;
                input_value_type L1 = in[3] & inmask;
                input_value_type K0 = in[4] & inmask;
                input_value_type K1 = in[5] & inmask;
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
                *result++ = R0;
                *result++ = L0;
                *result++ = R1;
                *result++ = L1;
            }
        // No more cases.  See the static_assert(n==2 || n==4) at the top of the class
        }
        return result;
    }

private:
    static constexpr array<output_value_type, n> MC = {consts...};
    static constexpr input_value_type inmask = detail::fffmask<input_value_type, input_word_size>;
}; 

// N.B.  The template param is 'int r' in P2075R1.  I think size_t is more consistent.
template <size_t r>
using philox2x32_prf_r = philox_prf<uint_fast32_t, 32, 2, r, 0xD256D193, 0x9E3779B9>;
template<size_t r>
using philox4x32_prf_r = philox_prf<uint_fast32_t, 32, 4, r, 0xD2511F53, 0x9E3779B9,
                                                    0xCD9E8D57, 0xBB67AE85>;

template <size_t r>
using philox2x64_prf_r = philox_prf<uint_fast64_t, 64, 2, r, 0xD2B74407B1CE6E93, 0x9E3779B97F4A7C15>;
template <size_t r>
using philox4x64_prf_r = philox_prf<uint_fast64_t, 64, 4, r, 0xD2E7470EE14C6C93, 0x9E3779B97F4A7C15,
                                                    0xCA5A826395121157, 0xBB67AE8584CAA73B>;

using philox2x32_prf = philox2x32_prf_r<10>;
using philox2x64_prf = philox2x64_prf_r<10>;
using philox4x32_prf = philox4x32_prf_r<10>;
using philox4x64_prf = philox4x64_prf_r<10>;

} // namespace std
