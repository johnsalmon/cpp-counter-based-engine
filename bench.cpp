#include "threefry_prf.hpp"
#include "philox_prf.hpp"
#include "counter_based_engine.hpp"
#include "timeit.hpp"
#include <iostream>
#include <chrono>
#include <ranges>
#include <map>
#include <functional>

using namespace std;
volatile int check = 0;

template<typename PRF>
void doit(string name){
    array<uint64_t, PRF::in_N> c = {99};
    auto perf = detail::timeit(std::chrono::seconds(5),
                                [&](){
                                    auto r = PRF{}(c);
                                    ranges::copy(r, begin(c));
                                });
    cout << "calling " << name << " directly: " << (c[0]==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << "Miters/sec\n";
    cout << "approx " << perf.sec_per_iter() *  3.e9 / sizeof(typename PRF::result_type) <<  " cycles per byte\n";

    counter_based_engine<PRF> engine;
    uint64_t r;
    perf = detail::timeit(chrono::seconds(5),
                           [&](){
                               r = engine();
                           });
    cout << "calling " << name << " through engine: " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << "Miters/sec\n";
    cout << "approx " << perf.sec_per_iter() *  3.e9 / sizeof(r) <<  " cycles per byte\n";
}

#define MAPPED(prf) {string(#prf), function<void(string)>(&doit<prf>)}
map<string, function<void(string)>> dispatch_map = {
    MAPPED(threefry4x64_prf<>),
    MAPPED(philox4x64_prf<>),
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
