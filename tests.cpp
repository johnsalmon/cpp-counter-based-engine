#include <iostream>
#include "counter_based_engine.hpp"
#include "philox_prf.hpp"
#include "threefry_prf.hpp"
#include <iostream>
#include <sstream>
#include <cassert>

// Save some typing:
using namespace std;
// N.B.  threefry has simd code inside it, so should be strictly more bug-prone
// than philox.
using eng_t = threefry4x64;
using prf_t = eng_t::prf_type;

// make it easy to print results:
template <typename T, size_t N>
std::ostream& operator<<(std::ostream& os, const array<T, N>& result){
    for(auto& r : result)
        os << r << " ";
    return os;
}

template <typename PRF, unsigned key_N>
void dokat(const std::string& s){
    std::istringstream iss(s);
    iss >> hex;
    using ranges::subrange;

    using in_type = PRF::in_type;
    in_type iv;
    using result_type = array<uintmax_t, PRF::result_N>;
    result_type result;
    for(size_t i=0; i<tuple_size<in_type>::value; ++i)
        iss >> iv[i];
    result_type reference;
    for(auto& r : reference)
        iss >> r;
    assert(iss);
    PRF prf;
    cout << hex;
    prf(ranges::single_view(iv), begin(result));
#if 1
    assert(result == reference);
#else
    cout << "result:    " << result << "\n";
    cout << "reference: " << reference << "\n";
#endif
    cout << "PASSED: " << s << endl;
}

int main(int argc, char **argv){
    // Known-answer tests from the original Random123 distribution.
    // The format is:  in[0 .. in_N] result[0 .. result_N]
    dokat<threefry2x32_prf<20>, 2>("00000000 00000000 00000000 00000000   6b200159 99ba4efe");
    dokat<threefry2x32_prf<20>, 2>("ffffffff ffffffff ffffffff ffffffff   1cb996fc bb002be7");
    dokat<threefry2x32_prf<20>, 2>("243f6a88 85a308d3 13198a2e 03707344   c4923a9c 483df7a0");
    dokat<threefry4x32_prf<20>, 4>("00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000   9c6ca96a e17eae66 fc10ecd4 5256a7d8");
    dokat<threefry4x32_prf<20>, 4>("ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff   2a881696 57012287 f6c7446e a16a6732");
    dokat<threefry4x32_prf<20>, 4>("243f6a88 85a308d3 13198a2e 03707344 a4093822 299f31d0 082efa98 ec4e6c89   59cd1dbb b8879579 86b5d00c ac8b6d84");

    dokat<threefry2x64_prf<20>, 2>("0000000000000000 0000000000000000 0000000000000000 0000000000000000   c2b6e3a8c2c69865 6f81ed42f350084d");
    dokat<threefry2x64_prf<20>, 2>("ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff   e02cb7c4d95d277a d06633d0893b8b68");
    dokat<threefry2x64_prf<20>, 2>("243f6a8885a308d3 13198a2e03707344 a4093822299f31d0 082efa98ec4e6c89   263c7d30bb0f0af1 56be8361d3311526");

    dokat<threefry4x64_prf<20>, 4>("0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000   09218ebde6c85537 55941f5266d86105 4bd25e16282434dc ee29ec846bd2e40b");
    dokat<threefry4x64_prf<20>, 4>("ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff 29c24097942bba1b 0371bbfb0f6f4e11 3c231ffa33f83a1c cd29113fde32d168");
    dokat<threefry4x64_prf<20>, 4>("243f6a8885a308d3 13198a2e03707344 a4093822299f31d0 082efa98ec4e6c89 452821e638d01377 be5466cf34e90c6c be5466cf34e90c6c c0ac29b7c97c50dd   a7e8fde591651bd9 baafd0c30138319b 84a5c1a729e685b9 901d406ccebc1ba4");

    dokat<threefry4x64_prf<13>, 4>("0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 4071fabee1dc8e05 02ed3113695c9c62 397311b5b89f9d49 e21292c3258024bc");
    dokat<threefry4x64_prf<13>, 4>("ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff 7eaed935479722b5 90994358c429f31c 496381083e07a75b 627ed0d746821121");
    dokat<threefry4x64_prf<13>, 4>("243f6a8885a308d3 13198a2e03707344 a4093822299f31d0 082efa98ec4e6c89 452821e638d01377 be5466cf34e90c6c c0ac29b7c97c50dd 3f84d5b5b5470917 4361288ef9c1900c 8717291521782833 0d19db18c20cf47e a0b41d63ac8581e5");

    dokat<philox2x64_prf<10>, 1>("0000000000000000 0000000000000000 0000000000000000   ca00a0459843d731 66c24222c9a845b5");
    dokat<philox2x64_prf<10>, 1>("ffffffffffffffff ffffffffffffffff ffffffffffffffff   65b021d60cd8310f 4d02f3222f86df20");
    dokat<philox2x64_prf<10>, 1>("243f6a8885a308d3 13198a2e03707344 a4093822299f31d0   0a5e742c2997341c b0f883d38000de5d");

    dokat<philox4x64_prf<10>, 2>("0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000 0000000000000000   16554d9eca36314c db20fe9d672d0fdc d7e772cee186176b 7e68b68aec7ba23b");
    dokat<philox4x64_prf<10>, 2>("ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff ffffffffffffffff   87b092c3013fe90b 438c3c67be8d0224 9cc7d7c69cd777b6 a09caebf594f0ba0");
    dokat<philox4x64_prf<10>, 2>("243f6a8885a308d3 13198a2e03707344 a4093822299f31d0 082efa98ec4e6c89 452821e638d01377 be5466cf34e90c6c   a528f45403e61d95 38c72dbd566e9788 a5a1610e72fd18b5 57bd43b5e52b7fe6");

    dokat<philox2x32_prf<10>, 1>("00000000 00000000 00000000   ff1dae59 6cd10df2");
    dokat<philox2x32_prf<10>, 1>("ffffffff ffffffff ffffffff   2c3f628b ab4fd7ad");
    dokat<philox2x32_prf<10>, 1>("243f6a88 85a308d3 13198a2e   dd7ce038 f62a4c12");

    dokat<philox4x32_prf<10>, 2>("00000000 00000000 00000000 00000000 00000000 00000000   6627e8d5 e169c58d bc57ac4c 9b00dbd8");
    dokat<philox4x32_prf<10>, 2>("ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff   408f276d 41c83b0e a20bc7c6 6d5451fd");
    dokat<philox4x32_prf<10>, 2>("243f6a88 85a308d3 13198a2e 03707344 a4093822 299f31d0   d16cfe09 94fdcceb 5001e420 24126ea1");
    cout << "PASSED: known-answer-tests" << endl;

    // Test discard and bulk generation - by far the trickiest corners
    // of the counter_based_engine implementation...
    eng_t jumpeng;
    // The cauchy distribution has a *very* fat tail (it has infinite variance!).
    // It's a good choice if we want to see some occasional very large jumps.
    cauchy_distribution<float> cd(0., 10.);
    eng_t eng1;
    eng_t eng2;
    eng_t eng3;
    const size_t max_jump = 10000;
    vector<eng_t::result_type> bulk(max_jump);
#if PRF_ALLOW_PERMUTED_RESULTS
    vector<eng_t::result_type> bulk2(max_jump);
#endif
    for(size_t i=0; i<1000000; ++i){
        size_t jump = min(max_jump, size_t(abs(cd(jumpeng))));
        
        eng1(&bulk[0], &bulk[jump]);
        for(size_t j=0; j<jump; ++j){
            auto r = eng2();
#if PRF_ALLOW_PERMUTED_RESULTS
            bulk2[j] = r;
#else
            assert(r == bulk[j]);
#endif
        }
#if PRF_ALLOW_PERMUTED_RESULTS
        sort(&bulk2[0], &bulk2[jump]);
        sort(&bulk[0], &bulk[jump]);
        for(size_t j=0; j<jump; ++j)
            assert(bulk[j] == bulk2[j]);
#endif
        eng3.discard(jump);
        assert(eng1 == eng2);
        assert(eng1 == eng3);
        assert(eng2 == eng3);
    }
    cout << "PASSED: discard/bulk tests" << endl;

    return 0;
}
