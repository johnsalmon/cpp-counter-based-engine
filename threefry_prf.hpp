#pragma once

#include "detail.hpp"
#include <cstdint>
#include <array>
#include <bit>
#include <ranges>

namespace std{

template<size_t  w, size_t n, size_t R, typename detail::uint_fast<w> ks_parity, int ... consts>
struct threefry_prf {
    static_assert(w>0);
    static_assert( n==2 || n==4, "N must be 2 or 4" );
    static_assert(sizeof ...(consts) == 4*n);

    using Uint = detail::uint_fast<w>;

    static constexpr array<int, 4*n> rotation_constants = {consts ...};

    // N.B.  can't use std::rotl because Uint might be wider than w.
    static constexpr Uint rotleft(Uint x, int r){
        return ((x<<r) | (x>>(w-r))) & inmask;
    }

    static inline void round2(Uint& c0, Uint& c1, unsigned r){
        c0 = (c0 + c1)&inmask; c1 = rotleft(c1,rotation_constants[r%8]); c1 ^= c0;
    }
    template <unsigned r>
    static inline void round2(Uint& c0, Uint& c1){
        c0 = (c0 + c1)&inmask; c1 = rotleft(c1,rotation_constants[r%8]); c1 ^= c0;
    }
    static inline void keymix2(Uint& c0, Uint& c1, Uint* ks, unsigned r){
        unsigned r4 = r>>2;
        c0 = (c0 + ks[r4%3]) & inmask; 
        c1 = (c1 + ks[(r4+1)%3] + r4) & inmask;
    }
    static void round4(Uint& c0, Uint& c1, Uint& c2, Uint& c3, unsigned r){
        if((r&1)==0){
            c0 = (c0+c1)&inmask; c1 = rotleft(c1,rotation_constants[r%8]); c1 ^= c0;
            c2 = (c2+c3)&inmask; c3 = rotleft(c3,rotation_constants[8+r%8]); c3 ^= c2;
        }else{
            c0 = (c0+c3)&inmask; c3 = rotleft(c3,rotation_constants[r%8]); c3 ^= c0;
            c2 = (c2+c1)&inmask; c1 = rotleft(c1,rotation_constants[8+r%8]); c1 ^= c2;
        }
    }
    template <unsigned r>
    static void round4(Uint& c0, Uint& c1, Uint& c2, Uint& c3){
        return round4(c0, c1, c2, c3, r);
        if((r&1)==0){
            c0 = (c0+c1)&inmask; c1 = rotleft(c1,rotation_constants[r%8]); c1 ^= c0;
            c2 = (c2+c3)&inmask; c3 = rotleft(c3,rotation_constants[8+r%8]); c3 ^= c2;
        }else{
            c0 = (c0+c3)&inmask; c3 = rotleft(c3,rotation_constants[r%8]); c3 ^= c0;
            c2 = (c2+c1)&inmask; c1 = rotleft(c1,rotation_constants[8+r%8]); c1 ^= c2;
        }
    }

    static void keymix4(Uint& c0, Uint& c1, Uint& c2, Uint& c3, Uint* ks, unsigned r){
        unsigned r4 = r>>2;
        c0 = (c0 + ks[(r4+0)%5]) & inmask; 
        c1 = (c1 + ks[(r4+1)%5]) & inmask;
        c2 = (c2 + ks[(r4+2)%5]) & inmask;
        c3 = (c3 + ks[(r4+3)%5] + r4) & inmask;
    }

public:
    static constexpr size_t in_bits = w;
    static constexpr size_t in_N = 2*n;
    static constexpr size_t result_bits = w;
    static constexpr size_t result_N = n;

    using in_value_type = Uint;
    using result_value_type = Uint;
    using result_type = array<result_value_type, result_N>;

    template <integral T>
    result_type operator()(initializer_list<T> il) const {
        return operator()(ranges::subrange(il));
    }

    // FIXME - is this the right signature?  InRange&& is a forwarding reference.
    template <detail::integral_input_range InRange>
    result_type operator()(InRange&& in) const{
        auto cp = ranges::begin(in);
        auto ce = ranges::end(in);
        Uint k[n+1];
        k[n] = ks_parity;
        for(unsigned i=0; i<n; ++i){
            k[i] = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            k[n] ^= k[i];
        }
        if constexpr (n == 2){
            in_value_type c0 = (cp==ce) ? 0 : in_value_type(*cp++);
            in_value_type c1 = (cp==ce) ? 0 : in_value_type(*cp++);
            
            c0 = (c0 + k[0])&inmask;
            c1 = (c1 + k[1])&inmask;
            // Surprisingly(?), gcc (through gcc10) doesn't unroll the
            // loop.  If we unroll it ourselves, it's about twice as fast.
#if 1
            if(R>0) round2<0>(c0, c1);
            if(R>1) round2<1>(c0, c1);
            if(R>2) round2<2>(c0, c1);
            if(R>3) round2<3>(c0, c1);
            if(R>3) keymix2(c0, c1, k, 4);

            if(R>4) round2<4>(c0, c1);
            if(R>5) round2<5>(c0, c1);
            if(R>6) round2<6>(c0, c1);
            if(R>7) round2<7>(c0, c1);
            if(R>7) keymix2(c0, c1, k, 8);

            if(R>8) round2<8>(c0, c1);
            if(R>9) round2<9>(c0, c1);
            if(R>10) round2<10>(c0, c1);
            if(R>11) round2<11>(c0, c1);
            if(R>11) keymix2(c0, c1, k, 12);
        
            if(R>12) round2<12>(c0, c1);
            if(R>13) round2<13>(c0, c1);
            if(R>14) round2<14>(c0, c1);
            if(R>15) round2<15>(c0, c1);
            if(R>15) keymix2(c0, c1, k, 16);

            if(R>16) round2<16>(c0, c1);
            if(R>17) round2<17>(c0, c1);
            if(R>18) round2<18>(c0, c1);
            if(R>19) round2<19>(c0, c1);
            if(R>19) keymix2(c0, c1, k, 20);
            static const unsigned UNROLLED=20;
#else
            static const unsigned UNROLLED=0;
#endif
            for(unsigned r=UNROLLED; r<R; ){
                round2(c0, c1, r);
                ++r;
                if((r&3)==0){
                    keymix2(c0, c1, k, r);
                }
            }
            return {c0, c1};
        }else if constexpr (n == 4) {
            in_value_type c0 = (cp==ce) ? 0 : in_value_type(*cp++);
            in_value_type c1 = (cp==ce) ? 0 : in_value_type(*cp++);
            in_value_type c2 = (cp==ce) ? 0 : in_value_type(*cp++);
            in_value_type c3 = (cp==ce) ? 0 : in_value_type(*cp++);

            c0 = (c0 + k[0])&inmask;
            c1 = (c1 + k[1])&inmask;
            c2 = (c2 + k[2])&inmask;
            c3 = (c3 + k[3])&inmask;

            // Surprisingly(?), gcc (through gcc10) doesn't unroll the
            // loop.  If we unroll it ourselves, it's about twice as fast.
#if 1
            if(R>0) round4<0>(c0, c1, c2, c3);
            if(R>1) round4<1>(c0, c1, c2, c3);
            if(R>2) round4<2>(c0, c1, c2, c3);
            if(R>3) round4<3>(c0, c1, c2, c3);
            if(R>3) keymix4(c0, c1, c2, c3, k, 4);

            if(R>4) round4<4>(c0, c1, c2, c3);
            if(R>5) round4<5>(c0, c1, c2, c3);
            if(R>6) round4<6>(c0, c1, c2, c3);
            if(R>7) round4<7>(c0, c1, c2, c3);
            if(R>7) keymix4(c0, c1, c2, c3, k, 8);

            if(R>8) round4<8>(c0, c1, c2, c3);
            if(R>9) round4<9>(c0, c1, c2, c3);
            if(R>10) round4<10>(c0, c1, c2, c3);
            if(R>11) round4<11>(c0, c1, c2, c3);
            if(R>11) keymix4(c0, c1, c2, c3, k, 12);
        
            if(R>12) round4<12>(c0, c1, c2, c3);
            if(R>13) round4<13>(c0, c1, c2, c3);
            if(R>14) round4<14>(c0, c1, c2, c3);
            if(R>15) round4<15>(c0, c1, c2, c3);
            if(R>15) keymix4(c0, c1, c2, c3, k, 16);

            if(R>16) round4<16>(c0, c1, c2, c3);
            if(R>17) round4<17>(c0, c1, c2, c3);
            if(R>18) round4<18>(c0, c1, c2, c3);
            if(R>19) round4<19>(c0, c1, c2, c3);
            if(R>19) keymix4(c0, c1, c2, c3, k, 20);
            static const unsigned UNROLLED = 20;
#else
            static const unsigned UNROLLED = 0;
#endif
            for(unsigned r=UNROLLED; r<R; ){
                round4(c0, c1, c2, c3, r);
                ++r;
                if((r&3)==0){
                    keymix4(c0, c1, c2, c3, k, r);
                }
            }
            return {c0, c1, c2, c3}; 
        }
        // No more cases.  See the static_assert(n==2 || n==4) at the top of the class.
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
