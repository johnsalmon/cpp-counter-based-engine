#include "threefry_prf.hpp"
#include "philox_prf.hpp"
#include "siphash_prf.hpp"
#include "counter_based_engine.hpp"
#include "timeit.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ranges>
#include <map>
#include <functional>
#include <algorithm>

using namespace std;
volatile int check = 0;

template <typename T, size_t N>
std::ostream& operator<<(std::ostream& os, const array<T, N>& a){
    for(const auto& v : a)
        os << v << " ";
    return os;
}

template<typename PRF>
void doit(string name){
    static constexpr size_t prf_output_count = PRF::output_count;
    static constexpr size_t prf_input_count = PRF::input_count;
    using prf_output_value_type = PRF::output_value_type;
    using prf_input_value_type = PRF::input_value_type;
    prf_output_value_type rprf = 0;
    timeit_result perf;
    static const size_t bulkN = 1024;
    static const size_t bits_per_byte = 8;
    float Gbytes_per_iter;

    // single (output_count) generation with prf
    prf_input_value_type c[prf_input_count] = {99};
    perf = timeit(std::chrono::seconds(5),
                       [&rprf, &c](){
                           prf_output_value_type rv[prf_output_count];
                           PRF{}(begin(c), begin(rv));
                           c[0]++;
                           rprf = accumulate(begin(rv), end(rv), rprf, bit_xor<decltype(rprf)>{});
                       });
    cout << "calling " << name << " directly (" << prf_output_count << " at a time): " << (rprf==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << " Miters/sec";
    Gbytes_per_iter = 1.e-9 * (PRF::output_word_size*prf_output_count)/bits_per_byte;
    cout << " approx " << perf.iter_per_sec() *  Gbytes_per_iter <<  " GB/s\n";

    // bulk generation directly with prf
    // prf_t::generate is a  bit tricky to call.  Is that a problem?
    static constexpr size_t Nprf = bulkN/prf_output_count;
    static_assert(bulkN%prf_output_count == 0);
    prf_input_value_type bulkin[Nprf*prf_input_count];
    for(size_t i=0; auto& v : bulkin)
        v = i++;  // fill bulkin with integers.

    auto range_of_ptrs_into_bulkin =
        views::iota(size_t(0), Nprf) |
        views::transform([&bulkin](auto i){
                             auto p  = &bulkin[0] + i*prf_input_count;
                             *p += 1; // *MODIFIES* bulkin.  So different randoms every time!
                             return p;
                         });
    
    perf = timeit(std::chrono::seconds(5),
                  [&rprf, &range_of_ptrs_into_bulkin](){
                      prf_output_value_type bulkout[bulkN];
                      PRF{}.generate(range_of_ptrs_into_bulkin,
                                     begin(bulkout));
                      rprf = accumulate(begin(bulkout), end(bulkout), rprf, bit_xor<decltype(rprf)>{});
                  });
    cout << "calling " << name << " directly (" << bulkN << " at a time): " << (rprf==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << " Miters/sec";
    Gbytes_per_iter = 1.e-9*(bulkN*PRF::output_word_size)/bits_per_byte;
    cout << " approx " << perf.iter_per_sec() *  Gbytes_per_iter <<  " GB/s\n";

    using engine_type = counter_based_engine<PRF, 64/PRF::input_word_size>;
    using engine_result_type = engine_type::result_type;
    static constexpr size_t engine_word_size = engine_type::word_size;
    engine_type engine;
    engine_result_type r = 0;
    perf = timeit(chrono::seconds(5),
                           [&](){
                               r ^= engine();
                           });
    cout << "calling " << name << " through engine (1 at a time): " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << " Miters/sec";
    Gbytes_per_iter = 1.e-9 * engine_word_size/bits_per_byte;
    cout << " approx " << perf.iter_per_sec() * Gbytes_per_iter <<  " GB/s\n";

    perf = timeit(chrono::seconds(5),
                           [&](){
                               array<engine_result_type, bulkN> bulk;
                               engine(begin(bulk), end(bulk));
                               r = accumulate(begin(bulk), end(bulk), r, bit_xor<engine_result_type>{});
                           });
    cout << "calling " << name << " through engine (" << bulkN << " at a  time): " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << " Miters/sec";
    Gbytes_per_iter = 1.e-9 * bulkN*engine_word_size/bits_per_byte;
    cout << " approx " << perf.iter_per_sec() * Gbytes_per_iter <<  " GB/s\n";
}

// a minimal prf that copies inputs to outputs - useful for estimating
// function call and related overheads
class null_prf{
public:
    static constexpr size_t input_word_size=64;
    static constexpr size_t input_count = 4;
    static constexpr size_t output_word_size=64;
    static constexpr size_t output_count = 4;
    using input_value_type = uint64_t;
    using output_value_type = uint64_t;
    template <detail::integral_input_range InRange, weakly_incrementable O>
    void operator()(InRange&& in, O result) const{
        ranges::copy(in, result);
    }
};        

#define MAPPED(prf) {string(#prf), function<void(string)>(&doit<prf>)}
#define _ ,
map<string, function<void(string)>> dispatch_map = {
                                                    //    MAPPED(uint64_t, null_prf),
    MAPPED(threefry4x64_prf),
    MAPPED(threefry2x64_prf),
    MAPPED(threefry4x32_prf),
    MAPPED(threefry2x32_prf),

    MAPPED(philox4x64_prf),
    MAPPED(philox2x64_prf),
    MAPPED(philox4x32_prf),
    MAPPED(philox2x32_prf),

    MAPPED(siphash_prf<4>),
    MAPPED(siphash_prf<16>),
};
    

int main(int argc, char**argv){
    cout << setprecision(2);
    if(argc == 1){
        for(auto& p : dispatch_map)
            p.second(p.first);
        return 0;
    }
    for(auto p = argv+1; *p; p++){
        auto found = dispatch_map.find(string(*p));
        if(found != dispatch_map.end())
            found->second(found->first);
        else
            cout << *p << " not found in dispatch map\n";
    }        
    return 0;
}
