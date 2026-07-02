# Limit Order Book

Production-style C++20 limit order book and matching engine intended for quant trading software engineering portfolios.

## Features

- Price-time priority matching
- Limit, market, cancel, and modify support
- Partial fills with emitted trade events
- Bid and ask books with level snapshots
- Deterministic replay from CSV input
- Replay CLI modes for event-stream and book-state inspection
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
./build/lob_replay --print-events --print-book data/crossing_orders.csv
```

Example output:

```text
commands=6 accepted=4 canceled=1 modified=1 rejected=1 trades=2 traded_quantity=80 traded_notional=8120
event=accepted order_id=1001 qty=80 price=100
...
best_bid{empty} best_ask{empty} bid_levels=0 ask_levels=0 bid_qty=0 ask_qty=0
```

## Benchmark Usage

```bash
./build/lob_bench
```

Output fields:

- `scenario`
- `throughput_ops_per_sec`
- `p50_ns`
- `p95_ns`
- `p99_ns`

## Benchmark Results

Placeholder:

```text
CPU: <fill in machine details>
Compiler: <fill in compiler and flags>
scenario=balanced throughput_ops_per_sec=<pending> p50_ns=<pending> p95_ns=<pending> p99_ns=<pending>
scenario=aggressive_buy throughput_ops_per_sec=<pending> p50_ns=<pending> p95_ns=<pending> p99_ns=<pending>
scenario=wide_spread throughput_ops_per_sec=<pending> p50_ns=<pending> p95_ns=<pending> p99_ns=<pending>
```

## Design Tradeoffs

- `std::map` gives deterministic best-price iteration and clean code, but a flatter custom ladder could be faster for dense price ranges.
- Intrusive lists remove per-level queue container overhead, but require careful pointer maintenance.
- Same-price quantity reductions preserve queue priority, while reprices and quantity increases requeue conservatively.
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
