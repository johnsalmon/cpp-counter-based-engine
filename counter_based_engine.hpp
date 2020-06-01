#pragma once
#include "detail.hpp"

#include <limits>
#include <array>
#include <random>
#include <iosfwd>
#include <algorithm>

namespace std{

template<typename PRF>
class counter_based_engine{
public:
    using in_value_type = PRF::in_value_type;
    using result_value_type = PRF::result_value_type;
    static constexpr size_t seed_bits = PRF::in_bits;
    static constexpr size_t result_bits = PRF::result_bits;
    static constexpr size_t seed_N = PRF::in_N - 1;
    static constexpr size_t result_N = PRF::result_N;

private:
    static constexpr size_t in_N = PRF::in_N;
    static constexpr size_t in_bits = PRF::in_bits;
    static_assert(numeric_limits<result_value_type>::max() >= result_N);
    // Other assertions, that we rely on, and that should be part of a PRF concept:
    //  {in_N, result_N} > 0
    //  {in_bits, result_bits} > 0
    //  unsigned_integral({key_value_type, in_value_type, result_value_type})

    static constexpr in_value_type in_mask = detail::fffmask<in_value_type, PRF::in_bits>;
    static constexpr result_value_type result_mask = detail::fffmask<result_value_type, result_bits>;

    array<in_value_type, PRF::in_N> in;
    PRF::result_type results;
    // To save space, store the index of the next result to be returned in the 0th results slot.
    const auto& ridxref() const {
        return results[0];
    }
    auto& ridxref() {
        return results[0];
    }

public:
    // First, satisfy the requirements for a uniform_random_bit_generator
    // result_type
    using result_type = result_value_type;
    // min, max
    static constexpr result_type min(){ return 0; }
    static constexpr result_type max(){ return result_mask; };
    // operator()
    result_type operator()(){
        auto ri = ridxref();
        if(ri >= result_N)
            throw out_of_range("counter_based_engine()");
        if(ri == 0){
            // call prf and increment ctr
            results = PRF{}(in);
            in.back() = (in.back() + 1) & in_mask;
        }
        result_type ret = results[ri++];
        if(ri == result_N){
            // check for overflow:  if the counter bits of in.back()
            // have wrapped around to zero, leave ri = result_N, which
            // will make the next call throw.
            if(in.back() != 0)
                [[likely]] ri = 0;
        }
        ridxref() = ri;
        return ret;
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
        // Generate enough to fill both a PRF::key and PRF::in
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
    void discard(unsigned long long jump) try {
        auto oldridx = ridxref();
        unsigned newridx = (jump + oldridx) % result_N;
        unsigned long long jumpll = jump + oldridx - (!oldridx && newridx);
        jumpll /= result_N;
        jumpll += !oldridx;
        in_value_type jumpctr = jumpll & in_mask;
        if( jumpll != jumpctr )
            [[unlikely]] throw out_of_range("counter_based_engine::discard");
        in_value_type oldctr = in.back();
        in_value_type newctr = (jumpctr-1 + oldctr) & in_mask;
        if(newctr < oldctr-1)
            [[unlikely]] throw out_of_range("counter_based_engine::discard");
        in.back() = newctr;
        if(newridx){
            if(jumpctr)
                results = PRF{}(in);
            in.back() = (in.back() + 1) & in_mask;
        }else if(newctr == 0){
            newridx = result_N;
        }
                
        ridxref() = newridx;
    }catch(out_of_range&){
        ridxref() = result_N;
        in.back() = 0; // so all overflown engines compare equal and serialize consistently.
        throw;
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
        copy_n(isiin, in_N, p.in.begin());
        result_value_type ridx;
        is >> ridx;
        if(ridx)
            p.results = PRF(p.in);
        p.ridxref() = ridx;
        return is;
    }

    // Extensions:

    // counter_based_engine has public methods and types that are not
    // required for a Uniform Random Number Engine:

    // - the type of the underlying PRF and a reference to it.
    using prf_type = PRF;

    // Constructor from an 'seed-range'
    template <detail::integral_input_range InRange>
    explicit counter_based_engine(InRange iv){
        seed(iv);
    }
    template <detail::integral_input_range InRange>
    void seed(InRange _in){
        // copy _in to in:
        auto inp = ranges::begin(_in);
        auto ine = ranges::end(_in);
        for(size_t i=0; i<seed_N; ++i)
            in[i] = (inp == ine) ? 0 : (*inp++) & in_mask; // ?? throw if *inp > in_mask??
        in.back() = 0;
        ridxref() = 0;
    }
    
    template <integral T>
    explicit counter_based_engine(initializer_list<T> il){
        seed(il);
    }
    template <integral T>
    void seed(initializer_list<T> il){
        seed(ranges::subrange(il));
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

} // namespace std
