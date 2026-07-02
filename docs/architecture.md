# Architecture

The engine uses price-level maps on each side of the book and intrusive FIFO queues per level to preserve price-time priority while keeping the hot path explicit and predictable.

Core ideas:

- `std::map<Price, PriceLevel>` keeps best-price discovery and level insertion simple and deterministic.
- Each `PriceLevel` stores aggregate quantity plus an intrusive linked list of resting orders.
- Orders live in a `std::deque<OrderNode>` so references remain stable without one heap allocation per order object.
- `std::unordered_map<OrderId, OrderNode*>` gives O(1) average cancel and modify lookups.
- Replay results capture accepted, canceled, modified, rejected, and trade events so event streams can be audited deterministically.
- Engine snapshots expose top-of-book and aggregate visible depth for lightweight debugging and replay inspection.

This is not the absolute lowest-latency design possible, but it is a clean portfolio-quality implementation with reasonable performance characteristics and readable code.
