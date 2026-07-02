# Limit Order Book

Production-style C++20 limit order book and matching engine intended for quant trading software engineering portfolios.

## Features

- Price-time priority matching
- Limit, market, cancel, and modify support
- Partial fills with emitted trade events
- Bid and ask books with level snapshots
- Deterministic replay from CSV input
- Unit tests with Catch2
- Benchmark executable reporting throughput, p50, p95, and p99 latency
- CMake-based build

## Project Layout

```text
include/   Public headers
src/       Engine and replay implementation
tests/     Unit tests
bench/     Benchmark executable
data/      Sample replay data
docs/      Design notes
```

## Architecture

The matching engine keeps two books:

- Bids: `std::map<Price, PriceLevel, std::greater<>>`
- Asks: `std::map<Price, PriceLevel, std::less<>>`

Each price level holds an intrusive FIFO list of `OrderNode` objects. Orders are stored in a `std::deque` to keep references stable and avoid one heap allocation per order on the hot path. An `std::unordered_map<OrderId, OrderNode*>` provides fast cancel and modify lookup.

This design intentionally favors clarity and deterministic behavior over highly specialized lock-free or cache-compressed structures. It is a practical baseline for a single-threaded matching engine that is straightforward to test, benchmark, and extend.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Replay Usage

```bash
./build/lob_replay data/sample_orders.csv
```

Example output:

```text
commands=6 trades=2 traded_quantity=80
```

## Benchmark Usage

```bash
./build/lob_bench
```

Output fields:

- `throughput_ops_per_sec`
- `p50_ns`
- `p95_ns`
- `p99_ns`

## Benchmark Results

Placeholder:

```text
CPU: <fill in machine details>
Compiler: <fill in compiler and flags>
throughput_ops_per_sec=<pending>
p50_ns=<pending>
p95_ns=<pending>
p99_ns=<pending>
```

## Design Tradeoffs

- `std::map` gives deterministic best-price iteration and clean code, but a flatter custom ladder could be faster for dense price ranges.
- Intrusive lists remove per-level queue container overhead, but require careful pointer maintenance.
- Modify operations currently requeue the order at the back of the new price level, which matches a conservative time-priority reset policy.
- The benchmark harness is custom and lightweight instead of depending on Google Benchmark.

## CSV Format

Expected header:

```text
timestamp,command,order_id,side,order_type,price,quantity,new_price,new_quantity
```

Commands:

- `add`: uses `side`, `order_type`, `price`, `quantity`
- `cancel`: uses `order_id`
- `modify`: uses `order_id`, `new_price`, `new_quantity`

## Next Extensions

- Add IOC/FOK support
- Add multi-symbol partitioning
- Add binary market data snapshots
- Add perf counters and allocator instrumentation
