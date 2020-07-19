#pragma once

#include "detail.hpp"
#include <cstdint>
#include <array>
#include <bit>
#include <ranges>

// If the "parallel" API allows the results to be returned permuted,
// we can write entire simd vectors to the output range.  This *might*
// allow for some additional optimization (but in practice, with gcc10
// on x86_64 with AVX-512, it seems not to make any difference)
#ifndef PRF_ALLOW_PERMUTED_RESULTS
#define PRF_ALLOW_PERMUTED_RESULTS 1
#endif

// Set SIMD_SIZE_BYTES to 0 to completely turn off SIMD.
#ifndef PRF_SIMD_SIZE_BYTES
// N.B. 32 bytes <-> AVX2, 64 bytes <-> AVX512
#define PRF_SIMD_SIZE_BYTES 64
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpsabi"

namespace std{

template<unsigned_integral UIntType, size_t  w, size_t n, size_t R, UIntType ks_parity, int ... consts>
class threefry_prf {
    static_assert(w <= numeric_limits<UIntType>::digits);
    static_assert( n==2 || n==4, "N must be 2 or 4" );
    static_assert(sizeof ...(consts) == 4*n);
    static_assert(R<=20, "loops are manually unrolled to R=20 only");

    static constexpr array<int, 4*n> rotation_constants = {consts ...};

    // The static methods are all templated on a Uint.  The
    // only instantiations will be with Uint=UIntType or
    // with Uint = a simd vector of UIntType.
    template <typename Uint>
    static constexpr Uint rotleft(Uint x, int r){
        return ((x<<r) | (x>>(w-r))) & inmask;
    }

    template <unsigned r, typename Uint>
    static inline void round2(Uint& c0, Uint& c1){
        c0 = (c0 + c1)&inmask; c1 = rotleft(c1,rotation_constants[r%8]); c1 ^= c0;
    }
    template <typename Uint>
    static void keymix2(Uint& c0, Uint& c1, Uint kk0, Uint kk1, unsigned r4){
        c0 = (c0 + kk0) & inmask; 
        c1 = (c1 + kk1 + r4) & inmask;
    }

    template <typename Uint>
    static inline void do2(Uint& cc0, Uint& cc1, Uint k0, Uint k1){
        auto c0 = cc0, c1 = cc1;
        Uint k2 = k0 ^ k1 ^ ks_parity;
        keymix2(c0, c1, k0, k1, 0);

        // Surprisingly(?), gcc (through gcc10) doesn't unroll the
        // loop.  If we unroll it ourselves, it's about twice as fast.
        if(R>0) round2<0>(c0, c1);
        if(R>1) round2<1>(c0, c1);
        if(R>2) round2<2>(c0, c1);
        if(R>3) round2<3>(c0, c1);
        if(R>3) keymix2(c0, c1, k1, k2, 1);

        if(R>4) round2<4>(c0, c1);
        if(R>5) round2<5>(c0, c1);
        if(R>6) round2<6>(c0, c1);
        if(R>7) round2<7>(c0, c1);
        if(R>7) keymix2(c0, c1, k2, k0, 2);

        if(R>8) round2<8>(c0, c1);
        if(R>9) round2<9>(c0, c1);
        if(R>10) round2<10>(c0, c1);
        if(R>11) round2<11>(c0, c1);
        if(R>11) keymix2(c0, c1, k0, k1, 3);
        
        if(R>12) round2<12>(c0, c1);
        if(R>13) round2<13>(c0, c1);
        if(R>14) round2<14>(c0, c1);
        if(R>15) round2<15>(c0, c1);
        if(R>15) keymix2(c0, c1, k1, k2, 4);

        if(R>16) round2<16>(c0, c1);
        if(R>17) round2<17>(c0, c1);
        if(R>18) round2<18>(c0, c1);
        if(R>19) round2<19>(c0, c1);
        if(R>19) keymix2(c0, c1, k2, k0, 5);

        cc0 = c0; cc1 = c1;
    }
    
    template <unsigned r, typename Uint>
    static void round4(Uint& c0, Uint& c1, Uint& c2, Uint& c3){
#define SIMPLIFY_PBOX 1
#if SIMPLIFY_PBOX
        auto c3tmp = c3;
        c0 = (c0+c1)&inmask; c3 = rotleft(c1,rotation_constants[r%8]); c3 ^= c0;
        c2 = (c2+c3tmp)&inmask; c1 = rotleft(c3tmp,rotation_constants[8+r%8]); c1 ^= c2;
#else
        if((r&1)==0){
            c0 = (c0+c1)&inmask; c1 = rotleft(c1,rotation_constants[r%8]); c1 ^= c0;
            c2 = (c2+c3)&inmask; c3 = rotleft(c3,rotation_constants[8+r%8]); c3 ^= c2;
        }else{
            c0 = (c0+c3)&inmask; c3 = rotleft(c3,rotation_constants[r%8]); c3 ^= c0;
            c2 = (c2+c1)&inmask; c1 = rotleft(c1,rotation_constants[8+r%8]); c1 ^= c2;
        }
#endif
    }

    template <typename Uint>
    static void keymix4(Uint& c0, Uint& c1, Uint& c2, Uint& c3, Uint kk0, Uint kk1, Uint kk2, Uint kk3, unsigned r4){
        c0 = (c0 + kk0) & inmask; 
        c1 = (c1 + kk1) & inmask;
        c2 = (c2 + kk2) & inmask;
        c3 = (c3 + kk3 + r4) & inmask;
    }

    template <typename Uint>
    static void do4(Uint& cc0, Uint& cc1, Uint& cc2,  Uint& cc3, Uint k0, Uint k1, Uint k2, Uint k3){
        auto c0 = cc0, c1 = cc1, c2 = cc2,  c3 = cc3;
        Uint k4  = k0 ^ k1 ^ k2 ^ k3 ^ ks_parity;
        keymix4(c0, c1, c2, c3, k0, k1, k2, k3, 0);

        // Surprisingly(?), gcc (through gcc10) doesn't unroll the
        // loop.  If we unroll it ourselves, it's about twice as fast.
        if(R>0) round4<0>(c0, c1, c2, c3);
        if(R>1) round4<1>(c0, c1, c2, c3);
        if(R>2) round4<2>(c0, c1, c2, c3);
        if(R>3) round4<3>(c0, c1, c2, c3);
        if(R>3) keymix4(c0, c1, c2, c3, k1, k2, k3, k4, 1);

        if(R>4) round4<4>(c0, c1, c2, c3);
        if(R>5) round4<5>(c0, c1, c2, c3);
        if(R>6) round4<6>(c0, c1, c2, c3);
        if(R>7) round4<7>(c0, c1, c2, c3);
        if(R>7) keymix4(c0, c1, c2, c3, k2, k3, k4, k0, 2);

        if(R>8) round4<8>(c0, c1, c2, c3);
        if(R>9) round4<9>(c0, c1, c2, c3);
        if(R>10) round4<10>(c0, c1, c2, c3);
        if(R>11) round4<11>(c0, c1, c2, c3);
        if(R>11) keymix4(c0, c1, c2, c3, k3, k4, k0, k1, 3);
        
        if(R>12) round4<12>(c0, c1, c2, c3);
        if(R>13) round4<13>(c0, c1, c2, c3);
        if(R>14) round4<14>(c0, c1, c2, c3);
        if(R>15) round4<15>(c0, c1, c2, c3);
        if(R>15) keymix4(c0, c1, c2, c3, k4, k0, k1, k2, 4);

        if(R>16) round4<16>(c0, c1, c2, c3);
        if(R>17) round4<17>(c0, c1, c2, c3);
        if(R>18) round4<18>(c0, c1, c2, c3);
        if(R>19) round4<19>(c0, c1, c2, c3);
        if(R>19) keymix4(c0, c1, c2, c3, k0, k1, k2, k3, 5);

#if SIMPLIFY_PBOX
        if(R&1){
            cc0 = c0; cc1 = c3; cc2 = c2; cc3 = c1;
        }else{
            cc0 = c0; cc1 = c1; cc2 = c2; cc3 = c3;
        }
#else
        cc0 = c0; cc1 = c1; cc2 = c2; cc3 = c3;
#endif
    }
public:
    using output_value_type = UIntType;
    using input_value_type = UIntType;
    static constexpr size_t input_word_size = w;
    static constexpr size_t output_word_size = w;
    static constexpr size_t input_count = 2*n;
    static constexpr size_t output_count = n;

    // In P2075R1 this returns void, but it makes more sense to return
    // the "final" OutputIterator2
    template<typename InputIterator1, typename OutputIterator2>
    OutputIterator2 operator()(InputIterator1 input, OutputIterator2 output){
        return generate(ranges::single_view(input), output);
    }

    // More constraints?  We currently require that the first argument to
    // operator():
    //  - satisfies input_range
    //  - satisfies sized_range
    //  - has a range_value_type that is an iterator of integers
    // and that  the second argument:
    //  - satisfies weakly_incrementable
    //  - is indirectly_writable
    //  - has an integral value_type
    template <ranges::input_range InRange, weakly_incrementable O>
    requires ranges::sized_range<InRange> &&
             integral<iter_value_t<ranges::range_value_t<InRange>>> &&
             integral<iter_value_t<O>> &&
             indirectly_writable<O, iter_value_t<O>>
    O generate(InRange&& in, O result) const{
        auto cp = ranges::begin(in);
        auto nleft = ranges::size(in);
        // N.B.  simd_size=64 gives some spurious warnings about 64-byte alignment
#if PRF_SIMD_SIZE_BYTES
        static constexpr int simd_size = PRF_SIMD_SIZE_BYTES;
        static constexpr size_t simd_N = simd_size/sizeof(input_value_type);
        while(nleft>simd_N){
            // N.B. some slots may be uninitialized, but we never use
            // the results from those slots, so it shouldn't matter.
            nleft -= simd_N;
            if constexpr (n == 2){
                input_value_type k0 __attribute__((vector_size(simd_size)));
                input_value_type k1 __attribute__((vector_size(simd_size)));
                input_value_type c0 __attribute__((vector_size(simd_size)));
                input_value_type c1 __attribute__((vector_size(simd_size)));

                for(unsigned s=0; s<simd_N; ++s){
                    auto initer = *cp++;
                    c0[s] = *initer++;
                    c1[s] = *initer++;
                    k0[s] = *initer++;
                    k1[s] = *initer++;
                }
                do2(c0, c1, k0, k1);
                // If we were allowed to permute the outputs and if
                // O::value_type is the same as Uint, then we could
                // avoid this "transpose" and copy the simd vectors
                // directly into *result.  Would that actually speed
                // things up?
#if PRF_ALLOW_PERMUTED_RESULTS
                for(unsigned s=0; s<simd_N; ++s) *result++ = c0[s];
                for(unsigned s=0; s<simd_N; ++s) *result++ = c1[s];
#else                
                for(unsigned s=0; s<simd_N; ++s){
                    *result++ = c0[s];
                    *result++ = c1[s];
                }
#endif // PRF_ALLOW_PERMUTED_RESULTS
            }else if constexpr (n == 4){
                input_value_type k0 __attribute__((vector_size(simd_size)));
                input_value_type k1 __attribute__((vector_size(simd_size)));
                input_value_type k2 __attribute__((vector_size(simd_size)));
                input_value_type k3 __attribute__((vector_size(simd_size)));
                input_value_type c0 __attribute__((vector_size(simd_size)));
                input_value_type c1 __attribute__((vector_size(simd_size)));
                input_value_type c2 __attribute__((vector_size(simd_size)));
                input_value_type c3 __attribute__((vector_size(simd_size)));
                for(unsigned s=0; s<simd_N; ++s){
                    auto initer = *cp++;
                    c0[s] = *initer++;
                    c1[s] = *initer++;
                    c2[s] = *initer++;
                    c3[s] = *initer++;
                    k0[s] = *initer++;
                    k1[s] = *initer++;
                    k2[s] = *initer++;
                    k3[s] = *initer++;
                }
                do4(c0, c1, c2, c3, k0, k1, k2, k3);
#if PRF_ALLOW_PERMUTED_RESULTS
                // write results in a permuted order, one entire simd-vector at a time.
                // Conceivably, the compiler might generate SIMD store instructions here,
                // which might improve performance.
                for(unsigned s=0; s<simd_N; ++s) *result++ = c0[s];
                for(unsigned s=0; s<simd_N; ++s) *result++ = c1[s];
                for(unsigned s=0; s<simd_N; ++s) *result++ = c2[s];
                for(unsigned s=0; s<simd_N; ++s) *result++ = c3[s];
#else
                // write results in exactly the same order as the inputs.
                for(unsigned s=0; s<simd_N; ++s){
                    *result++ = c0[s];
                    *result++ = c1[s];
                    *result++ = c2[s];
                    *result++ = c3[s];
                }
#endif // PRF_ALLOW_PERMUTED_RESULTS
            }
        }
#endif // PRF_SIMD_SIZE_BYTES

        while(nleft--){
            auto initer = *cp++;
            if constexpr (n == 2){
                input_value_type c0 = *initer++ ;
                input_value_type c1 = *initer++;
                input_value_type k0 = *initer++;
                input_value_type k1 = *initer++;
                do2(c0, c1, k0, k1);
                *result++ = c0;
                *result++ = c1;
            }else if constexpr (n == 4){
                input_value_type c0 = *initer++;
                input_value_type c1 = *initer++;
                input_value_type c2 = *initer++;
                input_value_type c3 = *initer++;
                input_value_type k0 = *initer++;
                input_value_type k1 = *initer++;
                input_value_type k2 = *initer++;
                input_value_type k3 = *initer++;
                do4(c0, c1, c2, c3, k0, k1, k2, k3);
                *result++ = c0;
                *result++ = c1;
                *result++ = c2;
                *result++ = c3;
            }
        }
        return result;
    }

private:
    static constexpr input_value_type inmask = detail::fffmask<input_value_type, input_word_size>;
};

// These constants are carefully chosen to achieve good randomization.  
// They are from Salmon et al <FIXME REF>.
//
// See Salmon et al, or Schneier's original work on Threefish <FIXME
// REF> for information about how the constants were obtained.

template<size_t r>
using threefry2x32_prf_r = threefry_prf<uint_least32_t, 32, 2, r, 0x1BD11BDA,
                                             13, 15, 26, 6, 17, 29, 16, 24>;

template<size_t r>
using threefry2x64_prf_r = threefry_prf<uint_least64_t, 64, 2, r, 0x1BD11BDAA9FC1A22,
                                             16, 42, 12, 31, 16, 32, 24, 21>;

template<size_t r>
using threefry4x32_prf_r = threefry_prf<uint_least32_t, 32, 4, r, 0x1BD11BDA,
                                             10, 11, 13, 23, 6, 17, 25, 18,
                                             26, 21, 27, 5, 20, 11, 10, 20>;

template<size_t r>
using threefry4x64_prf_r = threefry_prf<uint_least64_t, 64, 4, r, 0x1BD11BDAA9FC1A22,
                                             14, 52, 23, 5, 25, 46, 58, 32,
                                             16, 57, 40, 37, 33, 12, 22, 32>;
using threefry2x32_prf = threefry2x32_prf_r<20>;
using threefry2x64_prf = threefry2x64_prf_r<20>;
using threefry4x32_prf = threefry4x32_prf_r<20>;
using threefry4x64_prf = threefry4x64_prf_r<20>;

} // namespace std

#pragma GCC diagnostic pop
