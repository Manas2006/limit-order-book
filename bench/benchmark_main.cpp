#include "lob/matching_engine.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

namespace {

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

Metrics run_benchmark(std::size_t order_count) {
    lob::MatchingEngine engine {order_count};
    std::vector<std::uint64_t> latencies;
    latencies.reserve(order_count);

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> price_offset(-5, 5);
    std::uniform_int_distribution<int> quantity_dist(1, 100);

    const auto wall_start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < order_count; ++i) {
        const auto price = static_cast<lob::Price>(1000 + price_offset(rng));
        const auto side = side_dist(rng) == 0 ? lob::Side::buy : lob::Side::sell;
        const auto request = lob::OrderRequest {
            .order_id = static_cast<lob::OrderId>(i + 1),
            .side = side,
            .type = lob::OrderType::limit,
            .price = price,
            .quantity = static_cast<lob::Quantity>(quantity_dist(rng)),
            .timestamp = static_cast<lob::Timestamp>(i + 1),
        };

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
    const auto metrics = run_benchmark(order_count);

    std::cout << "orders=" << order_count << '\n'
              << "throughput_ops_per_sec=" << metrics.throughput_per_second << '\n'
              << "p50_ns=" << metrics.p50_ns << '\n'
              << "p95_ns=" << metrics.p95_ns << '\n'
              << "p99_ns=" << metrics.p99_ns << '\n';

    return 0;
}
