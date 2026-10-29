#include "../src/configs/configs.cpp"
#include <cassert>
#include <cstdio>

namespace
{
    configs::transport::service_result response{ true, 200, "{\"success\":true,\"configs\":[]}", "" };
    bool refresh_fails = false;
}
configs::transport::service_result configs::transport::post_json(const std::string& path, const std::string&)
{
    if (refresh_fails && path.ends_with("list")) return { false, 503, "", "Library temporarily unavailable." };
    return response;
}

int main()
{
    perform_action({ action_type::refresh });
    assert(current_snapshot.success && current_snapshot.revision == 1);
    refresh_fails = true;
    action publish_request{ action_type::publish };
    perform_action(publish_request);
    assert(current_snapshot.success && current_snapshot.status.starts_with("Config published publicly."));
    assert(current_snapshot.status.find("temporarily unavailable") != std::string::npos);
    response = { false, 403, "", "Not the owner." };
    perform_action({ action_type::remove, 3 });
    assert(!current_snapshot.success && current_snapshot.status == "Not the owner.");
    response = { true, 200, "{\"success\":true}", "" };
    perform_action({ action_type::load, 1 });
    assert(!current_snapshot.success && pending_payload.empty());
    response.body = "{\"success\":true,\"payload\":\"v3-test\",\"name\":\"Test\"}";
    perform_action({ action_type::load, 1 });
    assert(current_snapshot.success && pending_payload == "v3-test");
    std::string payload, name;
    assert(configs::consume_pending_payload(payload, name));
    assert(payload == "v3-test" && name == "Test" && !configs::consume_pending_payload(payload, name));
    pending_actions.push_back({ action_type::refresh });
    update_status("One request finished", true);
    assert(current_snapshot.busy && current_snapshot.revision == 6);
    pending_actions.clear();
    std::puts("Offline config publish, refresh failure, permission failure, pending payload and queue status checks passed.");
}
