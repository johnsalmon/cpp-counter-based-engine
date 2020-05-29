#pragma once

#include "detail.hpp" // a couple of helpful functions and concepts
#include <array>
#include <ranges>

namespace std{

// philox_keyed_prf models a keyed pseudo-random function by providing constant sizes
template<size_t w, size_t n, size_t r, typename detail::uint_fast<w> ... consts>
class philox_keyed_prf{
    static_assert(w>0);
    static_assert(n==2 || n==4); // FIXME - 8, 16?
    static_assert(sizeof ...(consts) == n);
    // assert that all constants are < 2^w ?
    using Uint = detail::uint_fast<w>;

public:
    static constexpr size_t key_bits = w;
    static constexpr size_t key_N = n/2;
    static constexpr size_t in_bits = w;
    static constexpr size_t in_N = n;
    static constexpr size_t result_bits = w;
    static constexpr size_t result_N = n;
    using key_value_type = Uint;
    using in_value_type = Uint;
    using result_value_type = Uint;
    using result_type = array<result_value_type, result_N>;

    // N.B.   All constructors call the corresponding rekey.
    philox_keyed_prf(){
        rekey();
    }
    void rekey(){
        rekey(ranges::views::empty<key_value_type>);
    }

    template <integral T>
    philox_keyed_prf(initializer_list<T> il){
        rekey(il);
    }
    template <integral T>
    void rekey(initializer_list<T> il){
        rekey(ranges::subrange(il));
    }

    // The value_types on KeyRange and InRange are not required to
    // have exactly {key,in}_bits.  If they're narrower, the
    // values are used as-is.  If they're wider, only the low w-bits
    // are used.  If they're signed, the values are converted to
    // unsigned.
    template <detail::integral_input_range KeyRange>
    philox_keyed_prf(KeyRange kin){
        rekey(kin);
    }

    template <detail::integral_input_range KeyRange>
    void rekey(KeyRange kin){
        auto kinp = ranges::begin(kin);
        auto kine = ranges::end(kin);
        for(auto& k : key)
            k = (kinp == kine) ? 0 : key_value_type(*kinp++)&keymask;
    }        

    template <integral T>
    result_type operator()(initializer_list<T> il){
        return operator()(ranges::subrange(il));
    }

    template <detail::integral_input_range InRange>
    result_type operator()(InRange in) const {
        static_assert(is_integral_v<ranges::range_value_t<InRange>>);
        auto cp = ranges::begin(in);
        auto ce = ranges::end(in);
        if constexpr (n == 2){
            auto [K0] = key;
            in_value_type R0 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            in_value_type L0 = (cp==ce) ? 0 : in_value_type(*cp++) & inmask;
            for(size_t i=0; i<r; ++i){
                auto [hi, lo] = detail::mulhilo<w>(R0, MC[0]);
                R0 = hi^K0^L0;
                L0 = lo;
                K0 = (K0+MC[1]) & keymask;
            }
            return {R0, L0};
        }else if constexpr (n == 4) {
            auto [K0,K1] = key;
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
                K0 = (K0 + MC[1]) & keymask;
                K1 = (K1 + MC[3]) & keymask;
            }
            return {R0, L0, R1, L1};
        }
        // No more cases.  See the static_assert(n==2 || n==4) at the top of the class
    }

    // (in)equality operators
    bool operator==(const philox_keyed_prf& rhs) const { return key == rhs.key; }
    bool operator!=(const philox_keyed_prf& rhs) const { return !operator==(rhs); }

    //  insertion/extraction operators
    template <typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>& operator<<(basic_ostream<CharT, Traits>& os, const philox_keyed_prf& p){
        // FIXME - save/restore os state
        ostream_iterator<key_value_type> osik(os, " ");
        copy(p.key.begin(), p.key.end(), osik);
        return os;
    }
    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>& operator>>(basic_istream<CharT, Traits>& is, philox_keyed_prf& p){
        // FIXME - save/restore is state
        istream_iterator<key_value_type> isik(is);
        copy_n(isik, key_N, p.key.begin());
        return is;
    }
private:
    static constexpr array<Uint, n> MC = {consts...};
    static constexpr key_value_type keymask = detail::fffmask<key_value_type, key_bits>;
    static constexpr in_value_type inmask = detail::fffmask<key_value_type, in_bits>;
    array<key_value_type, key_N> key;
}; 

using philox2x32_kprf = philox_keyed_prf<32, 2, 10, 0xD256D193, 0x9E3779B9>;
using philox4x32_kprf = philox_keyed_prf<32, 4, 10, 0xD2511F53, 0x9E3779B9,
                                                    0xCD9E8D57, 0xBB67AE85>;

using philox2x64_kprf = philox_keyed_prf<64, 2, 10, 0xD2B74407B1CE6E93, 0x9E3779B97F4A7C15>;
using philox4x64_kprf = philox_keyed_prf<64, 4, 10, 0xD2E7470EE14C6C93, 0x9E3779B97F4A7C15,
                                                    0xCA5A826395121157, 0xBB67AE8584CAA73B>;
} // namespace std

#include "counter_based_engine.hpp"
namespace std{

using philox2x32 = counter_based_engine<philox2x32_kprf>;
using philox4x32 = counter_based_engine<philox4x32_kprf>;
using philox2x64 = counter_based_engine<philox2x64_kprf>;
using philox4x64 = counter_based_engine<philox4x64_kprf>;

} // namespace std
