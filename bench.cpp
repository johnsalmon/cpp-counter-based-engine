#include "threefry_keyed_prf.hpp"
#include "philox_keyed_prf.hpp"
#include "counter_based_engine.hpp"
#include "timeit.hpp"
#include <iostream>
#include <chrono>
#include <ranges>
#include <map>
#include <functional>

using namespace std;
volatile int check = 0;

template<typename KPRF>
void doit(string name){
    array<uint64_t, KPRF::key_N> k = {99};
    array<uint64_t, KPRF::in_N> c = {};
    KPRF prf{k};
    auto perf = detail::timeit(std::chrono::seconds(5),
                                [&](){
                                    c = prf(c);
                                });
    cout << "calling " << name << " directly: " << (c[0]==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << "Miters/sec\n";
    cout << "approx " << perf.sec_per_iter() *  3.e9 / sizeof(c) <<  " cycles per byte\n";

    counter_based_engine<KPRF> engine;
    uint64_t r;
    perf = detail::timeit(chrono::seconds(5),
                           [&](){
                               r = engine();
                           });
    cout << "calling " << name << " through engine: " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << "Miters/sec\n";
    cout << "approx " << perf.sec_per_iter() *  3.e9 / sizeof(r) <<  " cycles per byte\n";
}

#define MAPPED(kprf) {string(#kprf), function<void(string)>(&doit<kprf>)}
map<string, function<void(string)>> dispatch_map = {
    MAPPED(threefry4x64_kprf<>),
    MAPPED(philox4x64_kprf<>),
};
    

int main(int argc, char**argv){
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
