#include "../src/modules/core/snapshot_session.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <latch>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

struct identity
{
    int world, player, handle;
    bool operator==(const identity& other) const { return world == other.world && player == other.player; }
};
struct overlay_fixture
{
    std::mutex mutex;
    modules::snapshot_session<identity> session;
    std::vector<int> markers;
    int scan_index = 0;

    std::uint64_t begin(identity owner)
    {
        std::lock_guard lock(mutex);
        if (session.observe(owner)) { markers.clear(); scan_index = 0; }
        return session.ticket();
    }
    bool publish(std::uint64_t ticket, int marker)
    {
        std::lock_guard lock(mutex);
        if (!session.accepts(ticket)) return false;
        markers = {marker}; scan_index = 128;
        return true;
    }
    std::vector<int> draw(std::optional<identity> live)
    {
        std::lock_guard lock(mutex);
        if (!live) { session.reset(); markers.clear(); scan_index = 0; return {}; }
        if (session.observe(*live)) { markers.clear(); scan_index = 0; return {}; }
        return markers;
    }
};
int main()
{
    const identity a{1, 10, 100}, same_a{1, 10, 999}, b{2, 20, 200}, respawn{2, 21, 201};
    overlay_fixture overlay;
    assert(overlay.draw(std::nullopt).empty());
    const auto first = overlay.begin(a);
    assert(overlay.publish(first, 7));
    assert(overlay.draw(same_a) == std::vector<int>{7}); // Different JNI handles, same Java objects.
    assert(overlay.session.ticket() == first);
    assert(overlay.draw(std::nullopt).empty()); // Disconnect with the module still enabled.
    assert(overlay.scan_index == 0);
    assert(!overlay.publish(first, 99)); // In-flight old-server scan cannot restore ghosts.
    assert(overlay.draw(std::nullopt).empty());
    const auto next = overlay.begin(b);
    assert(next != first && overlay.draw(b).empty());
    assert(overlay.publish(next, 8));
    assert(overlay.draw(b) == std::vector<int>{8});
    assert(overlay.draw(a).empty()); // Direct server switch without an intervening menu frame.
    assert(!overlay.publish(next, 88));
    const auto returning = overlay.begin(a);
    assert(overlay.publish(returning, 9));
    assert(overlay.draw(std::nullopt).empty());
    const auto same_world_rejoin = overlay.begin(a);
    assert(same_world_rejoin != returning && !overlay.publish(returning, 90));
    assert(overlay.publish(same_world_rejoin, 10));
    assert(overlay.draw(a) == std::vector<int>{10});
    const auto in_b = overlay.begin(b);
    assert(overlay.publish(in_b, 11));
    assert(overlay.draw(respawn).empty()); // Same world, replaced local player.
    assert(!overlay.publish(in_b, 111));

    std::latch started(1), disconnect_done(1);
    bool accepted = true;
    std::thread worker([&] {
        const auto ticket = overlay.begin(a);
        started.count_down();
        disconnect_done.wait();
        accepted = overlay.publish(ticket, 123);
    });
    started.wait();
    assert(overlay.draw(std::nullopt).empty());
    disconnect_done.count_down();
    worker.join();
    assert(!accepted && overlay.draw(std::nullopt).empty());
    const auto fresh = overlay.begin(b);
    assert(overlay.publish(fresh, 12) && overlay.draw(b) == std::vector<int>{12});
    overlay.session.reset();
    assert(!overlay.session.accepts(fresh) && !overlay.session.matches(b));
    const auto before_missing_jni = overlay.begin(b);
    overlay.session.invalidate(); // No JNI environment: hide immediately, retain refs until safe cleanup.
    assert(!overlay.session.accepts(before_missing_jni) && !overlay.session.matches(b));
    const auto after_missing_jni = overlay.begin(b);
    assert(after_missing_jni != before_missing_jni);
    std::cout << "Overlay sessions: disconnect, rapid switch, rejoin, respawn, JNI identity and late-worker rejection passed\n";
}
