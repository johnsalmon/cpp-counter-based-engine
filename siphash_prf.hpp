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

template <size_t w, size_t n>
class siphash_prf{
public:
    static constexpr size_t in_bits = w;
    static constexpr size_t in_N = n;
    static constexpr size_t result_bits = 64;
    static constexpr size_t result_N = 2;
    // Note that siphash.c assumes the existence of uint8_t and uint64_t
    // types, so there's no need jump through the hoops that would be
    // required if we tried to use uint_least8_t and uint_least64_t
    // instead.
    using result_value_type = uint64_t;
    using result_type = std::array<result_value_type, result_N>;
    
    using in_value_type = std::detail::uint_least<w>;

    template <std::integral T>
    result_type operator()(std::initializer_list<T> il) const {
        return operator()(std::ranges::subrange(il));
    }

    // Ignore endian issues!  This code will produce different
    // results on machines with different endian.  
    template <std::detail::integral_input_range InRange>
    result_type operator()(InRange in) const {
        const size_t bytes_per_in = (in_bits/8);
        const size_t inlen = std::ranges::size(in) * bytes_per_in;
        uint8_t inbytes[inlen];
        uint8_t *dest = &inbytes[0];
        for(auto v : in){
            ::memcpy(dest, &v, bytes_per_in);
            dest += bytes_per_in;
        }
        result_type out;
        siphash(&inbytes[16], inlen-16, &inbytes[0], (uint8_t*)&out[0], sizeof(out));
        return out;
    }
};
