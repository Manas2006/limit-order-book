#include "lob/matching_engine.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <string_view>
#include <vector>

namespace {

enum class ScenarioKind : std::uint8_t {
    balanced,
    aggressive_buy,
    wide_spread
};

struct ScenarioConfig {
    std::string_view name;
    ScenarioKind kind;
};

struct Metrics {
    double throughput_per_second {};
    double p50_ns {};
    double p95_ns {};
    double p99_ns {};
};

double percentile(std::vector<std::uint64_t> samples, double pct) {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<std::size_t>(pct * static_cast<double>(samples.size() - 1));
    return static_cast<double>(samples[index]);
}

lob::OrderRequest make_request(std::size_t index, ScenarioKind kind, std::mt19937_64& rng) {
    std::uniform_int_distribution<int> quantity_dist(1, 100);
    const auto timestamp = static_cast<lob::Timestamp>(index + 1);
    const auto order_id = static_cast<lob::OrderId>(index + 1);

    switch (kind) {
    case ScenarioKind::balanced: {
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_int_distribution<int> price_offset(-5, 5);
        return lob::OrderRequest {
            .order_id = order_id,
            .side = side_dist(rng) == 0 ? lob::Side::buy : lob::Side::sell,
            .type = lob::OrderType::limit,
            .price = static_cast<lob::Price>(1000 + price_offset(rng)),
            .quantity = static_cast<lob::Quantity>(quantity_dist(rng)),
            .timestamp = timestamp,
        };
    }
    case ScenarioKind::aggressive_buy: {
        std::uniform_int_distribution<int> side_roll(0, 9);
        std::uniform_int_distribution<int> price_offset(-2, 2);
        const auto is_buy = side_roll(rng) < 7;
        return lob::OrderRequest {
            .order_id = order_id,
            .side = is_buy ? lob::Side::buy : lob::Side::sell,
            .type = is_buy && (index % 5 == 0) ? lob::OrderType::market : lob::OrderType::limit,
            .price = is_buy ? static_cast<lob::Price>(1002 + price_offset(rng))
                            : static_cast<lob::Price>(1005 + price_offset(rng)),
            .quantity = static_cast<lob::Quantity>(quantity_dist(rng)),
            .timestamp = timestamp,
        };
    }
    case ScenarioKind::wide_spread: {
        const auto is_buy = (index % 2 == 0);
        std::uniform_int_distribution<int> price_offset(0, 4);
        return lob::OrderRequest {
            .order_id = order_id,
            .side = is_buy ? lob::Side::buy : lob::Side::sell,
            .type = lob::OrderType::limit,
            .price = is_buy ? static_cast<lob::Price>(990 - price_offset(rng))
                            : static_cast<lob::Price>(1010 + price_offset(rng)),
            .quantity = static_cast<lob::Quantity>(quantity_dist(rng)),
            .timestamp = timestamp,
        };
    }
    }

    return {};
}

Metrics run_benchmark(std::size_t order_count, ScenarioKind scenario) {
    lob::MatchingEngine engine {order_count};
    std::vector<std::uint64_t> latencies;
    latencies.reserve(order_count);

    std::mt19937_64 rng(42);

    const auto wall_start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < order_count; ++i) {
        const auto request = make_request(i, scenario, rng);

        const auto start = std::chrono::steady_clock::now();
        engine.submit(request);
        const auto stop = std::chrono::steady_clock::now();

        latencies.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count()));
    }
    const auto wall_stop = std::chrono::steady_clock::now();

    const auto elapsed_ns =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(wall_stop - wall_start).count());

    return Metrics {
        .throughput_per_second = elapsed_ns == 0.0 ? 0.0 : (static_cast<double>(order_count) * 1e9) / elapsed_ns,
        .p50_ns = percentile(latencies, 0.50),
        .p95_ns = percentile(latencies, 0.95),
        .p99_ns = percentile(latencies, 0.99),
    };
}

}  // namespace

int main() {
    constexpr std::size_t order_count = 200000;
    constexpr std::array<ScenarioConfig, 3> scenarios {{
        {"balanced", ScenarioKind::balanced},
        {"aggressive_buy", ScenarioKind::aggressive_buy},
        {"wide_spread", ScenarioKind::wide_spread},
    }};

    std::cout << "seed=42\n";
    for (const auto& scenario : scenarios) {
        const auto metrics = run_benchmark(order_count, scenario.kind);
        std::cout << "scenario=" << scenario.name << ' '
                  << "orders=" << order_count << ' '
                  << "throughput_ops_per_sec=" << metrics.throughput_per_second << ' '
                  << "p50_ns=" << metrics.p50_ns << ' '
                  << "p95_ns=" << metrics.p95_ns << ' '
                  << "p99_ns=" << metrics.p99_ns << '\n';
    }

    return 0;
}
