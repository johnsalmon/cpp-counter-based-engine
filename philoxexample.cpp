#include "counter_based_engine.hpp"
#include "philox_keyed_prf.hpp"
#include <cinttypes>
#include <iostream>
#include <vector>
#include <cassert>

// Save some typing:
using namespace std;
using eng_t = philox4x64;
using kprf_t = eng_t::kprf_type; // i.e., philox4x64_kprf;

// make it easy to print results:
std::ostream& operator<<(std::ostream& os, kprf_t::result_type& result){
    for(auto& r : result)
        os << r << " ";
    return os;
}

int main(int argc, char **argv){
    using ranges::subrange;

    eng_t eng1;  // default constructor of engine
    eng_t eng2(12345); // result_type constructor
    seed_seq ss{1, 2, 3, 4, 5}; 
    eng_t eng3(ss); // seed sequence constructor
    cout << "iter      eng1()           eng2()            eng3()\n";
    for(unsigned i=0; i<10; ++i){
        cout << i << " " << eng1() << " " << eng2() << " " << eng3() << "\n";
    }
    cout << "After 10 iterations, the three different engines are:\n";
    cout << "eng1: " << eng1 << "\n";
    cout << "eng2: " << eng2 << "\n";
    cout << "eng3: " << eng3 << "\n";

    // reseed eng1 with a result_type and then discard so it matches eng2
    eng1.seed(12345);
    eng1.discard(10);
    assert(eng1 == eng2);
    for(unsigned i=0; i<10; ++i)
        assert(eng1() == eng2());

    // reseed eng1 with a seedseq and then discard so it matches eng3
    eng1.seed(ss);
    eng1.discard(2); eng1.discard(8);
    assert(eng1 == eng3);
    for(unsigned i=0; i<10; ++i)
        assert(eng1() == eng3());

    cout << "Extract the kprf from eng3.\n";
    auto kprf1 = eng3.getkprf();
    cout << "The kprf serializes to: " << kprf1 << "\n";
    cout << "Call it directly  on {0, 0, 0, i}:\n";
    array<uint64_t, 4> in{};  // in full generality, array<kprf_t::in_value_type, kprf_t::in_N>
    for(unsigned i=0; i<10; ++i){
        auto result = kprf1(in);
        in[3]++;
        cout << i << ": " << result << "\n";
    }

    cout << "Construct a kprf with our own key and call it on {0, 0, 0, i}.  Expect different results:\n";
    array<uint64_t, 2> key2 = {1, 2}; // in full generality, array<kprf_t::key_value_type, kprf_t::key_N>
    kprf_t kprf2(key2);
    ranges::fill(in, 0); // reset in to all zeros
    for(unsigned i=0; i<10; ++i){
        auto result = kprf2(in);
        in[3]++;
        cout << i << ": " << result << "\n";
    }

    // Construct kprf3 from an initializer-list with the same values as kprf2
    kprf_t kprf3({1, 2});
    assert( kprf2 == kprf3 );

    cout << "Call the same kprf, but call it on {i, 2, 0, 0} instead.  Expect different results:\n";
    // This time, use a vector of only two unsigned (smaller than uint64_t) for the kprf input:
    std::vector<unsigned> in2{1, 2};
    for(unsigned i=0; i<10; ++i){
        auto result = kprf2(in2);
        assert(kprf3(in2) == result);
        in2[0]++;
        cout << i << ": " << result << "\n";
    }

    // The result is the same if we had explicitly provided all four 64-bit values:
    cout << "Generate the same  values, but with uint64_t in[4] instead of vector<unsigned> argument\n";
    uint64_t in3[] = {1, 2, 0, 0};
    in2[0] = 1; // reset in2
    for(unsigned i=0; i<10; ++i){
        auto result = kprf2(ranges::subrange(in3, in3+4));
        assert(result == kprf2(in2));
        in3[0]++;
        in2[0]++;
        cout << i << ": " << result << "\n";
    }

    cout << "And again, but with an initializer-list containing {i, 2}\n";
    int a=1, b=2;
    for(unsigned i=0; i<10; ++i){
        auto result = kprf2({a, b});
        // since c and d are 0, a shorter initialzer list produces the same results:
        assert(kprf2({a, b}) == result);
        a++;
        cout << i << ": " << result << "\n";
    }
    
    // Check that the example in proposal.txt actually works...
    {
        using kprf_t = philox4x64_kprf;
        kprf_t::key_value_type a = 1, b = 2;
        kprf_t kprf({a, b});     // 2^128 possible distinct kprf's
        // ...
        kprf_t::in_value_type c = 3, d=4, e=5; // 2^192 different distinct values
        auto eng = counter_based_engine(kprf, {c, d, e}); // 2^320 distinct engines
        eng(); eng();
    }

   // Suppose we had a monte carlo simulation that required three normally
    // distributed values for each of a large number of "atoms" at each of
    // a large number of timesteps.
    philox4x32_kprf kprf({999}); // global seed for entire "simulation"

    const int Ntimesteps = 2; // can be as large as 2^32
    const int Natoms = 3;     // can be as large as 2^32
    
    cout << "Generating normal random values from program state rather than sequentially:\n";
    cout << "t   aid      n1        n2         n3\n";
    for(uint32_t timestep  = 0; timestep < Ntimesteps; ++timestep){
        for(uint32_t atomid = 0; atomid < Natoms; ++atomid){
            // A single kprf can be shared by many threads.  "Time"
            // may run forwards or backwards.  Atoms may be "visited"
            // in any order and may be "visited" more than once per
            // timestep, etc.  In short, the statelessness of the
            // keyed_prf generator allows the overall algorithm
            // freedom that is not available when using a conventional
            // Random Number Generator.
            counter_based_engine<philox4x32_kprf> eng{kprf, {timestep, atomid}};
            normal_distribution nd;
            auto n1 = nd(eng);
            auto n2 = nd(eng);
            auto n3 = nd(eng);
            // Do something interesting with n1, n2, n3...
            cout << timestep << "   " << atomid << "    " << n1 << " " << n2 << " " << n3 << "\n";
        }
    }

    return 0;
}
