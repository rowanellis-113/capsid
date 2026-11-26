#include "host/channel_ledger.h"
#include "host/metrics.h"
#include "host/config.h"
#include "host/host_config_model.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

using namespace capsid::host;
namespace {
void require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
void hierarchical_slots_and_leases() {
    auto service = std::make_shared<ServiceChannelBudget>(ChannelBudgetLimits{1, 1, 1024});
    auto a = std::make_shared<ServiceChannelBudget>(ChannelBudgetLimits{2, 2, 800}, service);
    auto b = std::make_shared<ServiceChannelBudget>(ChannelBudgetLimits{2, 2, 800}, service);
    require(a->try_reserve_channel(), "listener A reserves service channel");
    require(!b->try_reserve_channel() && b->channels() == 0 && service->channels() == 1,
            "listener B cannot multiply service ceiling");
    require(service->snapshot().channels.rejected == 1 && b->snapshot().channels.rejected == 1,
            "both denying scopes count refusal");
    b->release_channel();
    require(service->channels() == 1, "rollback/duplicate release cannot steal A's slot");
    a->release_channel();
    require(b->try_reserve_channel(), "channel capacity returns for other listener");
    b->release_channel();
    require(a->try_reserve_dial() && !b->try_reserve_dial(),
            "pending dial ceiling spans listeners/executors");
    a->release_dial();
    require(b->try_reserve_dial(), "canceled dial returns capacity");
    b->release_dial();
    auto first = a->reserve_queue(600);
    require(bool(first), "first network buffer charged");
    require(!b->reserve_queue(500) && b->queued_bytes() == 0 && service->queued_bytes() == 600,
            "queue ancestor rejection rolls back child charge");
    auto writer = first;
    first.reset();
    a.reset();
    require(service->queued_bytes() == 600,
            "in-flight buffer retains queue lease and ancestor after session teardown");
    writer.reset();
    require(service->queued_bytes() == 0, "last writer returns ancestor bytes");
    auto second = b->reserve_queue(700);
    require(bool(second), "other listener reuses returned queue bytes");
    second.reset();
    auto snapshot = service->snapshot();
    require(snapshot.channels.high_water == 1 && snapshot.dials.high_water == 1 &&
                snapshot.queue.high_water == 700,
            "service high-water records actual reservations");
    require(!snapshot.channels.used && !snapshot.dials.used && !snapshot.queue.used,
            "all service reservations returned");
}
void activation_and_metrics() {
    auto service = std::make_shared<ServiceChannelBudget>();
    auto listener =
        std::make_shared<ServiceChannelBudget>(ChannelBudgetLimits{1, 0, 1024}, service);
    MetricsRegistry metrics;
    metrics.observe_channel_budget(service);
    metrics.observe_channel_budget(listener, "pub\n\"lic");
    metrics.observe_channel_budget(service); // static shards share one provider
    ChannelLedger ledger({}, listener.get());
    auto request = std::make_shared<ChannelLease>(ledger, true);
    auto ordinary = std::make_shared<ChannelLease>(ledger, false);
    require(request->valid() && ordinary->valid(), "separate HTTP and upgrade reservations");
    require(service->snapshot().http_inflight == 2 && service->channels() == 1 &&
                service->snapshot().active == 0,
            "pre-101 reservation is not active WS");
    request->commit_upgrade();
    request->commit_upgrade();
    require(service->snapshot().http_inflight == 1 && service->snapshot().active == 1,
            "101 releases HTTP exactly once and activates channel once");
    ordinary.reset();
    require(service->snapshot().http_inflight == 0 && service->snapshot().active == 1,
            "HTTP terminal keeps active WS");
    auto denied = std::make_shared<ChannelLease>(ledger, true);
    require(!denied->valid(), "listener ceiling rejects new upgrade");
    auto text = metrics.render_prometheus_text();
    require(text.find("capsid_channel_active{scope=\"service\",listener=\"\"} 1\n") !=
                std::string::npos,
            "live service gauge is rendered");
    require(text.find("listener=\"pub\\n\\\"lic\"") != std::string::npos,
            "configured listener name escaped as one Prometheus line");
    require(
        text.find(
            "capsid_channel_budget_rejections_total{resource=\"channels\",scope=\"listener\"") !=
            std::string::npos,
        "bounded resource/scope refusal counter");
    request->reject_upgrade();
    request->reject_upgrade();
    request.reset();
    denied.reset();
    require(!service->channels() && !service->snapshot().active &&
                !service->snapshot().http_inflight,
            "reject/cleanup cannot underflow or leave active metric");
    listener.reset();
    require(metrics.render_prometheus_text().find("pub\\n") == std::string::npos,
            "metrics weak provider does not retain retired listener");
}
void concurrent_capacity() {
    auto service = std::make_shared<ServiceChannelBudget>(ChannelBudgetLimits{7, 5, 1024});
    std::atomic<bool> ok{true};
    std::vector<std::thread> threads;
    for (int i = 0; i < 12; ++i)
        threads.emplace_back([&] {
            auto child =
                std::make_shared<ServiceChannelBudget>(ChannelBudgetLimits{20, 20, 1024}, service);
            for (int j = 0; j < 3000; ++j) {
                if (child->try_reserve_channel()) {
                    if (service->channels() > 7)
                        ok = false;
                    child->release_channel();
                }
                if (child->try_reserve_dial()) {
                    if (service->dials() > 5)
                        ok = false;
                    child->release_dial();
                }
                if (auto bytes = child->reserve_queue(256)) {
                    if (service->queued_bytes() > 1024)
                        ok = false;
                }
            }
        });
    for (auto& thread : threads)
        thread.join();
    auto snapshot = service->snapshot();
    require(ok && !snapshot.channels.used && !snapshot.dials.used && !snapshot.queue.used,
            "concurrent listener budgets remain bounded and return all leases");
    require(snapshot.channels.high_water <= 7 && snapshot.dials.high_water <= 5 &&
                snapshot.queue.high_water <= 1024,
            "concurrent high-water never exceeds service ceiling");
}
void config_projection() {
#ifndef _WIN32
    const std::string json =
        R"({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"isolation":{"mode":"strict","required":[]},"capacity":{"channelsTotal":3,"maxChannelMessageSize":"2MiB"},"listeners":[{"name":"public","tcp":"127.0.0.1:0","routing":{"mode":"path"},"limits":{"channels":2}}]})";
    require(validate_config_json(ConfigDocument::kHost, json).ok,
            "v2 service/listener schema fields accepted");
    ParsedHostConfig config;
    std::string error;
    const bool parsed = parse_host_config(json, &config, &error);
    require(parsed, error.c_str());
    require(config.capacity.channels.channels == 3 &&
                config.capacity.channels.pending_dials == 16 &&
                config.capacity.channels.queue_bytes == 32ULL * 1024ULL * 1024ULL,
            "v2 service fields map to derived enforced limits");
    require(config.listeners[0].limits.channels == 2 &&
                config.listeners[0].limits.channel_queue_bytes == 32ULL * 1024ULL * 1024ULL,
            "v2 listener fields map to enforced limits");
    const std::string zero_channels =
        R"({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":[{"name":"public","tcp":"127.0.0.1:0","routing":{"mode":"path"}}],"capacity":{"channelsTotal":0}})";
    require(!validate_config_json(ConfigDocument::kHost, zero_channels).ok,
            "operator count cannot disable finite quota with zero");
    for (const auto& size : {"0", "18446744073709551616B", "bogus"}) {
        auto bad = json;
        bad.replace(bad.find("2MiB"), 4, size);
        require(!parse_host_config(bad, &config, &error),
                "invalid/zero/overflow maxChannelMessageSize rejected at startup");
        require(error == "invalid host.json capacity",
                "size refusal comes from capacity parser, not another invalid fixture field");
    }
#endif
}
}
int main() {
    hierarchical_slots_and_leases();
    activation_and_metrics();
    concurrent_capacity();
    config_projection();
    std::puts(
        "channel budget: shared ancestors, buffer leases, distinct HTTP/active gauges, concurrency and schema PASS");
}
