#include "philox_keyed_prf.hpp"
#include "counter_based_engine.hpp"
#include "timeit.hpp"
#include <iostream>
#include <chrono>
#include <ranges>

using namespace std;
volatile int check = 0;
int main(int, char**){

    array<uint64_t, 2> k = {99, 88};
    array<uint64_t, 4> c = {};
    using ranges::subrange;
    philox4x64_kprf prf{k};
    auto perf = detail::timeit(std::chrono::seconds(5),
                                [&](){
                                    c = prf(c);
                                });
    std::cout << "calling mix directly:\n " << c[0] << "\n";
    std::cout << perf.iter_per_sec()/1e6 << "Miters/sec\n";
    std::cout << "approx " << perf.sec_per_iter() *  3.e9 / sizeof(c) <<  " cycles per byte\n";

    counter_based_engine<philox4x64_kprf> engine;
    uint64_t r;
    perf = detail::timeit(std::chrono::seconds(5),
                           [&](){
                               r = engine();
                           });
    std::cout << "calling through engine:\n " << r << "\n";
    std::cout << perf.iter_per_sec()/1e6 << "Miters/sec\n";
    std::cout << "approx " << perf.sec_per_iter() *  3.e9 / sizeof(r) <<  " cycles per byte\n";
    return 0;
}
