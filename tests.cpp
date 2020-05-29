#include "counter_based_engine.hpp"
#include "philox_keyed_prf.hpp"
#include <iostream>
#include <sstream>
#include <cassert>

// Save some typing:
using namespace std;
using eng_t = philox4x64;
using kprf_t = eng_t::kprf_type; // i.e., philox4x64_kprf;

// make it easy to print results:
template <typename T, size_t N>
std::ostream& operator<<(std::ostream& os, const array<T, N>& result){
    for(auto& r : result)
        os << r << " ";
    return os;
}

template <typename KPRF>
void dokat(const std::string& s){
    std::istringstream iss(s);
    iss >> hex;
    using ranges::subrange;

    std::array<typename KPRF::in_value_type, KPRF::in_N> iv;
    for(auto& i : iv)
        iss >> i;
    std::array<typename KPRF::key_value_type, KPRF::key_N> key;
    for(auto& k : key)
        iss >> k;
    typename KPRF::result_type reference;
    for(auto& r : reference)
        iss >> r;
    assert(iss);
    KPRF kprf(key);
    auto result = kprf(iv);
    assert(result == reference);
}

int main(int argc, char **argv){
    // A couple of known-answer tests from the original Random123 distribution.
    // The format is:  in[0 .. in_N] key[0 .. key_N] result[0 .. result_N]
    dokat<philox2x64_kprf>("0000000000000000 0000000000000000 0000000000000000   ca00a0459843d731 66c24222c9a845b5");
    dokat<philox2x64_kprf>("ffffffffffffffff ffffffffffffffff ffffffffffffffff   65b021d60cd8310f 4d02f3222f86df20");
    dokat<philox2x64_kprf>("243f6a8885a308d3 13198a2e03707344 a4093822299f31d0   0a5e742c2997341c b0f883d38000de5d");

    dokat<philox4x64_kprf>("0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000   16554d9eca36314c db20fe9d672d0fdc d7e772cee186176b 7e68b68aec7ba23b");
    dokat<philox4x64_kprf>("ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff   87b092c3013fe90b 438c3c67be8d0224 9cc7d7c69cd777b6 a09caebf594f0ba0");
    dokat<philox4x64_kprf>("243f6a8885a308d3 13198a2e03707344 a4093822299f31d0 082efa98ec4e6c89 452821e638d01377 be5466cf34e90c6c   a528f45403e61d95 38c72dbd566e9788 a5a1610e72fd18b5 57bd43b5e52b7fe6");

    dokat<philox2x32_kprf>("00000000 00000000 00000000   ff1dae59 6cd10df2");
    dokat<philox2x32_kprf>("ffffffff ffffffff ffffffff   2c3f628b ab4fd7ad");
    dokat<philox2x32_kprf>("243f6a88 85a308d3 13198a2e   dd7ce038 f62a4c12");

    dokat<philox4x32_kprf>("00000000 00000000 00000000 00000000 00000000 00000000   6627e8d5 e169c58d bc57ac4c 9b00dbd8");
    dokat<philox4x32_kprf>("ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff   408f276d 41c83b0e a20bc7c6 6d5451fd");
    dokat<philox4x32_kprf>("243f6a88 85a308d3 13198a2e 03707344 a4093822 299f31d0   d16cfe09 94fdcceb 5001e420 24126ea1");
    cout << "PASSED: known-answer-tests" << endl;

    // Test discard.  It's by far the trickiest piece of counter_based_engine
    eng_t eng1;
    eng_t eng2;
    for(size_t i=0; i<1000000; ++i){
        auto jump = eng1() % 12; eng2();
        //cout << "\nBefore eng1: " << eng1 << "\n";
        for(size_t j=0; j<jump; ++j)
            eng1();
        eng2.discard(jump);
        //cout << "jump: " << jump << "\n";
        //cout << "After eng1: " << eng1 << "\n";
        //cout << "After eng2: " << eng2 << "\n";
        assert(eng1 == eng2);
    }
    cout << "PASSED: discard" << endl;

    // FIXME - test out_of_range behavior.  (This was
    // much easeir when we could set  CTR_BITS=small and
    // easily exhaust a generator.  Running through 2^34
    // values at 200M/sec will take about a minute...
    using eng_t = counter_based_engine<philox4x32_kprf>;
    eng_t eng3;

    bool threw;
    cout << "It takes a couple of minutes to exhaust a 32-bit counter on a 3GHz machine.  Be patient..." << endl;
    auto Navail = uint64_t(eng_t::result_N)<<eng_t::in_bits;
    for(int loop : {1, 2}){
        for(uint64_t i=0; i<Navail; ++i)
            eng3();
        // The next call should throw
        threw = false;
        try{
            eng3();
        }catch(out_of_range&){
            threw = true;
        }
        assert(threw);

        // Let's try that again.  Should continue to throw
        threw = false;
        try{
            eng3();
        }catch(out_of_range&){
            threw = true;
        }
        assert(threw);
        // Let's reseed.  Shouldn't throw...
        cout << "PASSED:  out_of_range test " << loop << endl;
        eng3.seed();
    }

    
    return 0;
}
