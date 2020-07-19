#include "counter_based_engine.hpp"
#include "philox_prf.hpp"
#include <cinttypes>
#include <iostream>
#include <vector>
#include <cassert>

// Save some typing:
using namespace std;
using eng_t = philox4x64;
using prf_t = eng_t::prf_type; // i.e., philox4x64_prf;

// make it easy to print results:
template<typename T, size_t N>
std::ostream& operator<<(std::ostream& os, array<T, N>& result){
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

    cout << "Extract the prf from eng3.\n";
    using prf_t = decltype(eng3)::prf_type;
    prf_t prf1;
    cout << "Call it directly  on {0, 0, 0, 0, 0, i}:\n";
    array<uint64_t, 6> in{};  // in full generality, array<prf_t::input_value_type, prf_t::input_count>
    array<uint64_t, 4> result;
    for(unsigned i=0; i<10; ++i){
        prf1(begin(in), begin(result));
        in[5]++;
        cout << i << ": " << result << "\n";
    }

    
    // Check that the examples in README.md actually work...
    {
        using prf_t = philox4x64_prf;
        static const size_t Nin  = 3;
        array<prf_t::input_value_type, prf_t::input_count> in[Nin] = {{1,2,3,4,5,6}, {7,8, 9,10,11,12}, {13,14,15,16,17,18}};
        uint64_t out[Nin*prf_t::output_count]; // 12 values
        prf_t{}.generate(in | views::transform([](auto& a){return begin(a);}), begin(out));
        cout << "Random values obtained directly from philox4x64_prf: ";
        for(const auto o : out)
            cout << o << " ";
        cout << "\n";

        uint64_t a = 1, b = 2, c = 3, d=4, e=5;
        auto eng = counter_based_engine<prf_t, 1>({a, b, c, d, e}); // 2^320 distinct engines, each with period 2^66
        eng(); eng();
    }

    // Suppose we had a monte carlo simulation that required three normally
    // distributed values for each of a large number of "atoms" at each of
    // a large number of timesteps.
    const int Ntimesteps = 2; // can be as large as 2^32
    const int Natoms = 3;     // can be as large as 2^32
    
    cout << "Generating normal random values from program state rather than sequentially:\n";
    cout << "t   aid      n1        n2         n3\n";
    uint32_t global_seed = 999;
    for(uint32_t timestep  = 0; timestep < Ntimesteps; ++timestep){
        for(uint32_t atomid = 0; atomid < Natoms; ++atomid){
            // A single prf can be shared by many threads.  "Time"
            // may run forwards or backwards.  Atoms may be "visited"
            // in any order and may be "visited" more than once per
            // timestep, etc.  In short, the statelessness of the
            // keyed_prf generator allows the overall algorithm
            // freedom that is not available when using a conventional
            // Random Number Generator.
            counter_based_engine<philox4x32_prf, 1> eng{{global_seed, timestep, atomid}};
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
