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
    dokat<philox4x64_kprf>("0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000   16554d9eca36314c db20fe9d672d0fdc d7e772cee186176b 7e68b68aec7ba23b");
    dokat<philox4x64_kprf>("ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff   87b092c3013fe90b 438c3c67be8d0224 9cc7d7c69cd777b6 a09caebf594f0ba0");
    dokat<philox4x64_kprf>("243f6a8885a308d3 13198a2e03707344 a4093822299f31d0 082efa98ec4e6c89 452821e638d01377 be5466cf34e90c6c   a528f45403e61d95 38c72dbd566e9788 a5a1610e72fd18b5 57bd43b5e52b7fe6");

    dokat<philox2x32_kprf>("00000000 00000000 00000000   ff1dae59 6cd10df2");
    dokat<philox2x32_kprf>("ffffffff ffffffff ffffffff   2c3f628b ab4fd7ad");
    dokat<philox2x32_kprf>("243f6a88 85a308d3 13198a2e   dd7ce038 f62a4c12");

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

    static const unsigned CTR_BITS=6;
    counter_based_engine<philox4x64_kprf, CTR_BITS> eng3;
    bool threw;
    long Navail = (decltype(eng3)::kprf_type::result_N<<CTR_BITS);
    for(int loop : {1, 2}){
        (void) loop;
        for(long i=0; i<Navail; ++i)
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
        eng3.seed();
    }

    // Let's try that discard loop on eng3, checking that
    // the throw the same way and they leave the engine
    // in the same state.
    eng3.seed({0},  {0});
    auto eng4 = eng3;
    bool eng3_threw, eng4_threw;
    for(size_t i=0; i<10000000; ++i){
        auto jump = eng3() % 18; eng4();
        //cout << hex << "\nBefore eng3: " << eng3 << "\n";
        eng3_threw = false;
        try{
            for(size_t j=0; j<jump; ++j)
                eng3();
        }catch(out_of_range&){
            eng3_threw = true;
        }
        eng4_threw = false;
        try{
            eng4.discard(jump);
        }catch(out_of_range&){
            eng4_threw = true;
        }
        //cout << "jump: " << jump << "\n";
        //cout << "After eng3: " << (eng3_threw?"threw ":"") << eng3 << "\n";
        //cout << "After eng4: " << (eng4_threw?"threw ":"") << eng4 << "\n";
        assert(eng3_threw == eng4_threw);
        assert(eng3 == eng4);
        if(eng3_threw){
            eng3.seed({0}, {0});
            eng4.seed({0}, {0});
        }
    }
    
    return 0;
}
