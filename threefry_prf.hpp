#pragma once

#include "detail.hpp"
#include <cstdint>
#include <array>
#include <bit>
#include <ranges>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpsabi"

namespace std{

template<size_t  w, size_t n, size_t R, typename detail::uint_fast<w> ks_parity, int ... consts>
class threefry_prf {
    static_assert(w>0);
    static_assert( n==2 || n==4, "N must be 2 or 4" );
    static_assert(sizeof ...(consts) == 4*n);
    static_assert(R<=20, "loops are manually unrolled to R=20 only");

    static constexpr array<int, 4*n> rotation_constants = {consts ...};

    // The static methods are all templated on a Uint, which
    // can either be a bona fide unsigned integer type OR
    // a simd vector of unsigned integers.
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
        if((r&1)==0){
            c0 = (c0+c1)&inmask; c1 = rotleft(c1,rotation_constants[r%8]); c1 ^= c0;
            c2 = (c2+c3)&inmask; c3 = rotleft(c3,rotation_constants[8+r%8]); c3 ^= c2;
        }else{
            c0 = (c0+c3)&inmask; c3 = rotleft(c3,rotation_constants[r%8]); c3 ^= c0;
            c2 = (c2+c1)&inmask; c1 = rotleft(c1,rotation_constants[8+r%8]); c1 ^= c2;
        }
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

        cc0 = c0; cc1 = c1; cc2 = c2; cc3 = c3;
    }
    using in_value_type = detail::uint_fast<w>;
public:
    static constexpr size_t in_bits = w;
    using in_type = array<in_value_type, 2*n>;
    static constexpr size_t result_bits = w;
    static constexpr size_t result_N = n;

    // More constraints?  We currently require that the first argument to
    // operator():
    //  - satisfies input_range
    //  - satisfies sized_range
    //  - has a value_type that is_same as in_type, i.e., the value_type is an array
    // and that  the second argument:
    //  - satisfies weakly_incrementable
    //  - is indirectly_writable
    //  - has an integral value_type
    template <ranges::input_range InputRangeOfInTypes, weakly_incrementable O>
    requires ranges::sized_range<InputRangeOfInTypes> &&
             is_same_v<iter_value_t<ranges::iterator_t<InputRangeOfInTypes>>, in_type> &&
             integral<iter_value_t<O>> &&
             indirectly_writable<O, iter_value_t<O>>
    O operator()(InputRangeOfInTypes&& in, O result) const{
        auto cp = ranges::begin(in);
        auto nin = ranges::size(in);
        auto nleft = nin;
        // N.B.  simd_size=64 gives some spurious warnings about 64-byte alignment
#if 1 // is this actually a performance win??
        static constexpr int simd_size = 64; // 32 bytes <-> AVX2, 64 bytes <-> AVX512
        static constexpr size_t simd_N = simd_size/sizeof(in_value_type);
        while(nleft>simd_N){
            // N.B. some slots may be uninitialized, but we never use
            // the results from those slots, so it shouldn't matter.
            nleft -= simd_N;
            if constexpr (n == 2){
                in_value_type k0 __attribute__((vector_size(simd_size)));
                in_value_type k1 __attribute__((vector_size(simd_size)));
                in_value_type c0 __attribute__((vector_size(simd_size)));
                in_value_type c1 __attribute__((vector_size(simd_size)));

                for(unsigned s=0; s<simd_N; ++s){
                    const in_type& next = *cp++;
                    c0[s] = next[0];
                    c1[s] = next[1];
                    k0[s] = next[2];
                    k1[s] = next[3];
                }
                do2(c0, c1, k0, k1);
                // If we were allowed to permute the outputs and if
                // O::value_type is the same as Uint, then we could
                // avoid this "transpose" and copy the simd vectors
                // directly into *result.  Would that actually speed
                // things up?
                for(unsigned s=0; s<simd_N; ++s){
                    *result++ = c0[s];
                    *result++ = c1[s];
                }
            }else if constexpr (n == 4){
                in_value_type k0 __attribute__((vector_size(simd_size)));
                in_value_type k1 __attribute__((vector_size(simd_size)));
                in_value_type k2 __attribute__((vector_size(simd_size)));
                in_value_type k3 __attribute__((vector_size(simd_size)));
                in_value_type c0 __attribute__((vector_size(simd_size)));
                in_value_type c1 __attribute__((vector_size(simd_size)));
                in_value_type c2 __attribute__((vector_size(simd_size)));
                in_value_type c3 __attribute__((vector_size(simd_size)));
                for(unsigned s=0; s<simd_N; ++s){
                    const in_type& next = *cp++;
                    c0[s] = next[0];
                    c1[s] = next[1];
                    c2[s] = next[2];
                    c3[s] = next[3];
                    k0[s] = next[4];
                    k1[s] = next[5];
                    k2[s] = next[6];
                    k3[s] = next[7];
                }
                do4(c0, c1, c2, c3, k0, k1, k2, k3);
                // Question:  is there anything to be gained by
                // avoiding this "transpose"?
                for(unsigned s=0; s<simd_N; ++s){
                    *result++ = c0[s];
                    *result++ = c1[s];
                    *result++ = c2[s];
                    *result++ = c3[s];
                }
            }
        }
#endif

        while(nleft--){
            const in_type& next = *cp++;
            if constexpr (n == 2){
                auto [c0, c1, k0, k1] = next; // all are in_value_type
                do2(c0, c1, k0, k1);
                *result++ = c0;
                *result++ = c1;
            }else if constexpr (n == 4){
                auto [c0, c1, c2, c3, k0, k1, k2, k3] = next; // all are in_value_type
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
    static constexpr in_value_type inmask = detail::fffmask<in_value_type, in_bits>;
};

// These constants are carefully chosen to achieve good randomization.  
// They are from Salmon et al <FIXME REF>.
//
// See Salmon et al, or Schneier's original work on Threefish <FIXME
// REF> for information about how the constants were obtained.

// FIXME - the template alias lets the program choose a different value of R (good)
// but it also forces the programmer to add an empty template parameter list if
// the default value is desired (bad).  Some additional syntactic sugar is needed.
template<unsigned R=20>
using threefry2x32_prf = threefry_prf<32, 2, R, 0x1BD11BDA,
                                             13, 15, 26, 6, 17, 29, 16, 24>;

template<unsigned R=20>
using threefry2x64_prf = threefry_prf<64, 2, R, 0x1BD11BDAA9FC1A22,
                                             16, 42, 12, 31, 16, 32, 24, 21>;

template<unsigned R=20>
using threefry4x32_prf = threefry_prf<32, 4, R, 0x1BD11BDA,
                                             10, 11, 13, 23, 6, 17, 25, 18,
                                             26, 21, 27, 5, 20, 11, 10, 20>;

template<unsigned R=20>
using threefry4x64_prf = threefry_prf<64, 4, R, 0x1BD11BDAA9FC1A22,
                                             14, 52, 23, 5, 25, 46, 58, 32,
                                             16, 57, 40, 37, 33, 12, 22, 32>;
} // namespace std

#pragma GCC diagnostic pop
