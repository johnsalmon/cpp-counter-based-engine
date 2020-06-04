#include "threefry_prf.hpp"
#include "philox_prf.hpp"
#include "siphash_prf.hpp"
#include "counter_based_engine.hpp"
#include "timeit.hpp"
#include <iostream>
#include <chrono>
#include <ranges>
#include <map>
#include <functional>
#include <algorithm>

using namespace std;
volatile int check = 0;

float ticks_per_sec = 3.e9;

template<typename PRF>
void doit(string name){
    array<uint64_t, PRF::in_N> c = {99};
    using result_value_type = PRF::result_type::value_type;
    result_value_type r;
    auto perf = timeit(std::chrono::seconds(5),
                       [&r, &c](){
                           auto rv = PRF{}(c);
                           c.back()++;
                           r = accumulate(begin(rv), end(rv), r, bit_xor<result_value_type>{});
                       });
    cout << "calling " << name << " directly: " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << "Miters/sec\n";
    cout << "approx " << perf.sec_per_iter() *  ticks_per_sec / sizeof(typename PRF::result_type) <<  " cycles per byte\n";

    counter_based_engine<PRF> engine;
    perf = timeit(chrono::seconds(5),
                           [&](){
                               r ^= engine();
                           });
    cout << "calling " << name << " through engine: " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << "Miters/sec\n";
    cout << "approx " << perf.sec_per_iter() *  ticks_per_sec / sizeof(r) <<  " cycles per byte\n";
}

// a minimal prf that copies inputs to outputs - useful for estimating
// function call and related overheads
class null_prf{
public:
    static constexpr size_t in_bits=64;
    static constexpr size_t in_N = 4;
    static constexpr size_t result_bits=64;
    static constexpr size_t result_N = 4;
    using result_value_type = uint64_t;
    using in_value_type = uint64_t;
    using result_type = array<result_value_type, result_N>;
    template <detail::integral_input_range InRange>
    result_type operator()(InRange&& in) const{
        result_type ret;
        copy_n(begin(in), 4, begin(ret));
        return ret;
    }
};        

#define MAPPED(prf) {string(#prf), function<void(string)>(&doit<prf>)}
#define _ ,
map<string, function<void(string)>> dispatch_map = {
    MAPPED(null_prf),
    MAPPED(threefry4x64_prf<>),
    MAPPED(philox4x64_prf<>),
    MAPPED(siphash_prf<32 _ 4>),
};
    

int main(int argc, char**argv){
    cout << "CPB results assume a 3GHz clock!\n";
    ticks_per_sec = 3.e9;
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
