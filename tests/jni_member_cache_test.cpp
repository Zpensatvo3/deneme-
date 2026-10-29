#include "../src/platform/jni/member_id_cache.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

struct shared_signature;
struct other_signature;
using cache = platform::jni_detail::member_id_cache<int, int, shared_signature>;
using other_cache = platform::jni_detail::member_id_cache<int, int, other_signature>;

int main()
{
    int resolutions = 0;
    const auto lookup = [&](int owner) { return cache::get(owner, [&] { ++resolutions; return owner * 11; }); };
    assert(lookup(1) == 11 && lookup(2) == 22 && lookup(1) == 11);
    assert(lookup(3) == 33 && lookup(2) == 22 && resolutions == 3);
    assert(cache::get(0, [&] { assert(false); return -1; }) == 0);
    assert(cache::get(4, [] { return 0; }) == 0);
    assert(cache::get(4, [] { assert(false); return 44; }) == 0);
    assert(other_cache::get(1, [] { return 99; }) == 99);
    assert(lookup(1) == 11);
    // Resolution may reenter another owner of the same binding without a cache lock.
    assert(cache::get(5, [] { return cache::get(6, [] { return 66; }) + 1; }) == 67);
    std::atomic<bool> valid = true;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread) threads.emplace_back([&] {
        for (int iteration = 0; iteration < 10000; ++iteration)
            for (int owner = 1; owner <= 3; ++owner)
                if (cache::get(owner, [] { assert(false); return -1; }) != owner * 11) valid = false;
    });
    for (auto& thread : threads) thread.join();
    assert(valid);
    std::cout << "JNI owner cache: class isolation, misses, signature isolation, reentrancy and concurrent readers passed\n";
}
