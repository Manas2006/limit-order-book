#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace lob {

using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::uint64_t;
using Timestamp = std::uint64_t;

enum class Side : std::uint8_t {
    buy,
    sell
};

enum class OrderType : std::uint8_t {
    limit,
    market
};

enum class CommandType : std::uint8_t {
    add,
    cancel,
    modify
};

enum class EventType : std::uint8_t {
    accepted,
    canceled,
    modified,
    rejected,
    trade
};

struct OrderRequest {
    OrderId order_id {};
    Side side {};
    OrderType type {OrderType::limit};
    Price price {};
    Quantity quantity {};
    Timestamp timestamp {};
};

struct ModifyRequest {
    OrderId order_id {};
    Price new_price {};
    Quantity new_quantity {};
    Timestamp timestamp {};
};

struct CancelRequest {
    OrderId order_id {};
    Timestamp timestamp {};
};

struct ReplayCommand {
    CommandType type {};
    std::optional<OrderRequest> add;
    std::optional<CancelRequest> cancel;
    std::optional<ModifyRequest> modify;
};

struct TradeEvent {
    OrderId resting_order_id {};
    OrderId incoming_order_id {};
    Side aggressor_side {};
    Price price {};
    Quantity quantity {};
    Timestamp timestamp {};
};

struct EngineEvent {
    EventType type {};
    OrderId order_id {};
    Quantity quantity {};
    Price price {};
    std::string reason;
    std::optional<TradeEvent> trade;
};

struct BookLevel {
    Price price {};
    Quantity aggregate_quantity {};
    std::size_t order_count {};
};

inline constexpr std::string_view to_string(Side side) {
    return side == Side::buy ? "buy" : "sell";
}

inline constexpr std::string_view to_string(OrderType type) {
    return type == OrderType::limit ? "limit" : "market";
}

inline constexpr std::string_view to_string(EventType type) {
    switch (type) {
    case EventType::accepted:
        return "accepted";
    case EventType::canceled:
        return "canceled";
    case EventType::modified:
        return "modified";
    case EventType::rejected:
        return "rejected";
    case EventType::trade:
        return "trade";
    }
    return "unknown";
}

}  // namespace lob
