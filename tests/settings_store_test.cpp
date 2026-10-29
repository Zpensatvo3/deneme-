#include "../src/modules/core/settings_store.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>

struct record
{
    int count = 0;
    int opposite = 0;
    bool operator==(const record&) const = default;
};

int main()
{
    modules::settings::store<record> settings;
    auto copy = settings.read();
    copy.count = 42;
    assert(copy.count == 42 && settings.read().count == 0);
    assert(!settings.update([](record& value) { value.count = 5; return false; }));
    assert(settings.read() == record{});
    try
    {
        settings.update([](record& value) -> bool { value.count = 8; throw std::runtime_error("reject"); });
        assert(false);
    }
    catch (const std::runtime_error&) {}
    assert(settings.read() == record{});
    assert(!settings.update([](record&) { return true; }));

    std::atomic<bool> start{false}, done{false}, inconsistent{false};
    std::atomic<int> samples{0};
    auto writer = [&] {
        while (!start.load()) std::this_thread::yield();
        for (int i = 0; i < 5000; ++i)
            settings.update([](record& value) {
                ++value.count;
                value.opposite = -value.count;
                return true;
            });
    };
    std::thread first(writer), second(writer);
    std::thread reader([&] {
        while (!start.load()) std::this_thread::yield();
        do
        {
            const auto value = settings.read();
            if (value.opposite != -value.count) inconsistent.store(true);
            ++samples;
        } while (!done.load());
    });
    start.store(true);
    first.join(); second.join();
    done.store(true);
    reader.join();
    assert(!inconsistent.load() && samples.load() > 0);
    assert(settings.read().count == 10000 && settings.read().opposite == -10000);
    std::cout << "Settings store: rollback, copy isolation, coherent snapshots and serialized writers passed\n";
}
