#pragma once
#include "detail.hpp"

#include <limits>
#include <array>
#include <random>
#include <iosfwd>
#include <algorithm>
#include <cstring>
#include <vector>
#include "threefry_prf.hpp"
#include "philox_prf.hpp"

namespace std{

template<typename PRF, size_t CounterWords>
class counter_based_engine{
    static_assert(numeric_limits<typename PRF::result_type>::max() >= PRF::result_N);
    static_assert(CounterWords > 0);
    // FIXME! static_assert(CounterWords * PRF::in_bits <= numeric_limits<uintmax_t>::digits); // could be relaxed with more code
    // assertions that should be part of a PRF concept:
    static_assert(PRF::in_bits > 0);
    static_assert(PRF::result_bits > 0);
    static_assert(PRF::result_N > 0);
    //  PRF::in_type is std::array-like (subscriptable, has an integral value_type, specializes tuple_size)
public:
    using result_type = PRF::result_type;
    static constexpr size_t result_bits = PRF::result_bits;
    static constexpr size_t counter_words = CounterWords;
private:
    using prf_result_type = array<result_type, PRF::result_N>;
    static constexpr size_t result_N = PRF::result_N;

    using in_type = PRF::in_type;
    static constexpr size_t in_N = tuple_size<in_type>::value;
    static constexpr size_t in_bits = PRF::in_bits;
    using in_value_type = in_type::value_type;
    static_assert(integral<in_value_type>);

    static constexpr auto in_mask = detail::fffmask<in_value_type, PRF::in_bits>;
    static constexpr auto result_mask = detail::fffmask<result_type,
                                                        std::min<size_t>(numeric_limits<result_type>::digits, result_bits)>;

    in_type in;
    prf_result_type results;
    // To save space, store the index of the next result to be returned in the 0th results slot.
    const auto& ridxref() const {
        return results[0];
    }
    auto& ridxref() {
        return results[0];
    }

    // Some methods to manipulate a (possibly) multi-word counter in
    // the firs few elements of in[].  An integral counter_type isn't
    // strictly necessary.  We could do all the counter arithmetic by
    // carefully carrying bits between the first few counter_words
    // elements of in[].  But this is a lot easier.
    // WARNING:  the code is poorly tested.
    using counter_type = detail::uint_fast<CounterWords*PRF::in_bits>;
    counter_type get_counter() const{
        uint64_t ret = 0;
        for(size_t i=0; i<counter_words; ++i)
            ret |= uint64_t(in[i])<<(in_bits * i);
        return ret;
    }
    static void set_counter(in_type& inn, counter_type newctr){
        static_assert(in_bits * counter_words <= numeric_limits<counter_type>::digits);
        for(size_t i=0; i<counter_words; ++i)
            inn[i] = (newctr >> (in_bits*i)) & in_mask;
    }
    void incr_counter(){
        in[0] = (in[0] + 1) & in_mask;
        for(size_t i=1; i<counter_words; ++i){
            if(in[i-1])
                [[likely]]return;
            in[i] = (in[i] + 1) & in_mask;
        }
    }

public:
    // First, satisfy the requirements for a uniform_random_bit_generator
    // result_type - defined above
    // min, max
    static constexpr result_type min(){ return 0; }
    static constexpr result_type max(){ return result_mask; };
    // operator()
    result_type operator()(){
#if 0
        // N.B.  Writing it out doesn't seem to be an faster than
        // letting the compiler optimize away all the loops
        // in the bulk-generation overload.
        auto ri = ridxref();
        if(ri == 0){
            // call prf and increment ctr
            PRF{}(in, begin(results));
            incr_counter();
        }
        result_type ret = results[ri++];
        if(ri == result_N)
            ri = 0;
        ridxref() = ri;
        return ret;
#else
        result_type ret;
        (*this)(&ret, &ret+1);
        return ret;
#endif        
    }
            
    // constraints borrowed from ranges::fill ??
    // FIXME - overflow checking.  If we do it,
    // then the overflow behavior should be as if
    // we had done:
    //     while(n--) *out++ = (*this)();
    // Worth the trouble?
    template <output_iterator<const result_type&> O, sized_sentinel_for<O> S>
    O operator()(O out, S sen){
        auto n = sen - out;
        
        // Deliver any saved results
        auto ri = ridxref();
        if(ri && n){
            while(ri < result_N && n){
                *out++ = results[ri++];
                --n;
            }
            if(ri == result_N)
                ri = 0;
        }
            
        // Call the bulk generator
        auto nprf = n/result_N;
        // lazily construct the input range.  No need
        // to allocate and fill a big chunk of memory
        using namespace std::ranges;
        auto c0 = get_counter();
        out = PRF{}(views::iota(c0, c0+nprf) |
                    views::transform([&](auto ctr){
                                         auto inn = in;
                                         set_counter(inn, ctr);
                                         return inn;
                                     }),
                    out);
        n -= nprf*result_N;
        set_counter(in, c0 + nprf);

        // Restock the results array
        if(ri == 0 && n){
            PRF{}(ranges::single_view(in), std::begin(results));
            incr_counter();
        }
            
        // Finish off any stragglers.
        while(n--)
            *out++ = results[ri++];
        ridxref() = ri;
        return out;
    }

    // And now, the requirements for a random number engine:
    // constructors, seed and assignment methods:
    explicit counter_based_engine(){ seed(); }
    explicit counter_based_engine(result_type s){ seed(s); }
    template <typename SeedSeq> // FIXME - disambiguate result_type
    explicit counter_based_engine(SeedSeq& q){ seed(q); }
    void seed(){ seed_seq sss; seed(sss); }
    void seed(result_type s){ seed_seq sss{{s}}; seed(sss); }
    template <typename SeedSeq> // FIXME - disambiguate result_type
    void seed(SeedSeq& s){
        // Generate 32-bits at a time with the SeedSeq.
        // Generate enough to fill PRF::in
        constexpr size_t N32_per_in = (in_bits-1)/32 + 1;
        array<uint_fast32_t, N32_per_in * seed_N> k32;
        s.generate(k32.begin(), k32.end());

        auto k32p = begin(k32);
        array<in_value_type, seed_N> iv;
        for(auto& v : iv){
            v = 0;
            for(size_t j=0; j<N32_per_in; ++j)
                v |= in_value_type(*k32p++) << (32*j);
            v &= in_mask;
        }            

        seed(iv);
    }

    // (in)equality operators
    bool operator==(const counter_based_engine& rhs) const { return in == rhs.in && ridxref() == rhs.ridxref(); }
    bool operator!=(const counter_based_engine& rhs) const { return !operator==(rhs); }

    // discard - N.B.  There are a lot of tricky corner cases here
    //  that have NOT been tested.  E.g., really large jumps and/or
    //  an in_value_type that's wider than w.
    void discard(unsigned long long jump) {
        auto oldridx = ridxref();
        unsigned newridx = (jump + oldridx) % result_N;
        unsigned long long jumpll = jump + oldridx - (!oldridx && newridx);
        jumpll /= result_N;
        jumpll += !oldridx;
        in_value_type jumpctr = jumpll & in_mask;
        in_value_type oldctr = get_counter();
        in_value_type newctr = (jumpctr-1 + oldctr) & in_mask;
        set_counter(in, newctr);
        if(newridx){
            if(jumpctr)
                PRF{}(ranges::single_view(in), begin(results));
            incr_counter();
        }else if(newctr == 0){
            newridx = result_N;
        }
                
        ridxref() = newridx;
    }

    // stream inserter and extractor
    template <typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>& operator<<(basic_ostream<CharT, Traits>& os, const counter_based_engine& p){
        // FIXME - save/restore os state
        ostream_iterator<in_value_type> osin(os, " ");
        ranges::copy(p.in, osin);
        return os << p.ridxref();
    }
    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>& operator>>(basic_istream<CharT, Traits>& is, counter_based_engine& p){
        // FIXME - save/restore is state
        istream_iterator<in_value_type> isiin(is);
        copy_n(isiin, in_N, begin(p.in));
        result_type ridx;
        is >> ridx;
        if(ridx)
            PRF{}(ranges::single_view(p.in), begin(p.results));
        p.ridxref() = ridx;
        return is;
    }

    // Extensions:

    // counter_based_engine has public methods and types that are not
    // required for a Uniform Random Number Engine:

    // - the type of the underlying PRF and a reference to it.
    using prf_type = PRF;

    // - how many values are consumed in the seed(InRange) member
    // and corresponding constructor?
    static constexpr size_t seed_N = in_N - counter_words;
    static constexpr size_t seed_bits = PRF::in_bits;
    // N.B.  result_bits is declared at the top.
    
    // Constructors and seed members from from a 'seed-range'
    template <integral T>
    explicit counter_based_engine(initializer_list<T> il){
        seed(il);
    }
    template <integral T>
    void seed(initializer_list<T> il){
        seed(ranges::subrange(il));
    }
    
    template <detail::integral_input_range InRange>
    explicit counter_based_engine(InRange iv){
        seed(iv);
    }
    template <detail::integral_input_range InRange>
    void seed(InRange _in){
        // copy _in to in:
        auto inp = ranges::begin(_in);
        auto ine = ranges::end(_in);
        for(size_t i=counter_words; i<in_N; ++i)
            in[i] = (inp == ine) ? 0 : (*inp++) & in_mask; // ?? throw if *inp > in_mask??
        set_counter(in, 0);
        ridxref() = 0;
    }
    

    // Additional possible extensions:
    //
    // - a method to get the iv.
    // - methods that return the sequence length and how many calls are left.  N.B.  these
    //   would need a way to return a value larger than numeric_limits<uintmax_t>::max().
    // - a rewind() method (the opposite of discard).
    // - a const operator[](ull N) that returns the Nth value,
    //   leaving the state alone.
    // - a seek(ull) method to set the internal counter.
    
};

using philox2x32 = counter_based_engine<philox2x32_prf<>, 2>;
using philox4x32 = counter_based_engine<philox4x32_prf<>, 2>;
using philox2x64 = counter_based_engine<philox2x64_prf<>, 1>;
using philox4x64 = counter_based_engine<philox4x64_prf<>, 1>;

using threefry2x32 = counter_based_engine<threefry2x32_prf<>, 2>;
using threefry4x32 = counter_based_engine<threefry4x32_prf<>, 2>;
using threefry2x64 = counter_based_engine<threefry2x64_prf<>, 1>;
using threefry4x64 = counter_based_engine<threefry4x64_prf<>, 1>;

} // namespace std
