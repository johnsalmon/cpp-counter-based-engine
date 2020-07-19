#pragma once

// This is *not* proposed PRF for inclusion in the C++ standard.
// Instead, it is meant to demonstrate how a program could adapt
// a well-known, well-tested pseudo-random function (the "reference
// implementation" of SipHash in siphash.c), so that it can be used
// with the proposed counter_based_engine.
//
// N.B.  There are numerous implementations of SipHash (see
// https://131002.net/siphash/#sw) with their own API's and
// performance characteristics.  We've chosen to adapt siphash.c
// because it's a short, clean, "reference" implementation, not
// because it is particularly fast or particularly well-suited to
// being adapted into a PRF.

#include "detail.hpp"
#include <array>
#include <ranges>
#include <cstring>

extern "C"{
// see siphash.c
int siphash(const uint8_t *in, const size_t inlen, const uint8_t *k,
            uint8_t *out, const size_t outlen);
}

template <size_t n>
class siphash_prf{
    static_assert(n > 2);
public:
    // The API in siphash.c is expressed in terms of uint8_t.  But
    // the algorithm is "really" expressed in terms of 64-bit
    // arithmetic.  So let's expose that.  The number of 64-bit
    // inputs is the template parameter, n.
    using input_value_type = uint64_t;
    using output_value_type = uint64_t;
    static constexpr size_t input_word_size = 64;
    static constexpr size_t output_word_size = 64;
    static constexpr size_t input_count = n;
    static constexpr size_t output_count = 2;
    // Note that siphash.c assumes the existence of uint8_t and uint64_t
    // types, so there's no need jump through the hoops that would be
    // required if we tried to use uint_least8_t and uint_least64_t
    // instead.
    
    template<typename InputIterator1, typename OutputIterator2>
    OutputIterator2 operator()(InputIterator1 input, OutputIterator2 output){
        return generate(std::ranges::single_view(input), output);
    }

    // Ignore endian issues!  This code will produce different
    // results on machines with different endian.  
    template <std::ranges::input_range InRange, std::weakly_incrementable O>
    requires std::ranges::sized_range<InRange> &&
             std::integral<std::iter_value_t<std::ranges::range_value_t<InRange>>> &&
             std::integral<std::iter_value_t<O>> &&
             std::indirectly_writable<O, std::iter_value_t<O>>
    O generate(InRange&& inrange, O result) const{
        for(auto in : inrange){
            uint64_t out[2];
            // Call siphash with the first 16 bytes (in[0] and in[1]) 
            // as the key and the rest (in[2], ... ) as the message.
            uint64_t key[2] = {*in++, *in++};
            uint64_t msg[input_count-2];
            std::copy_n(in, input_count-2, msg);
            siphash(16+(uint8_t*)&key[0], sizeof(msg), (uint8_t*)&msg[0], (uint8_t*)&out[0], sizeof(out));
            *result++ = out[0];
            *result++ = out[1];
        }
        return result;
    }
};
