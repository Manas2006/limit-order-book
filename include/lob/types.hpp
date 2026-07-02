#pragma once

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

struct BookSnapshot {
    std::optional<BookLevel> best_bid;
    std::optional<BookLevel> best_ask;
    Quantity total_bid_quantity {};
    Quantity total_ask_quantity {};
    std::size_t bid_levels {};
    std::size_t ask_levels {};
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

inline std::string format_level(const std::optional<BookLevel>& level) {
    if (!level.has_value()) {
        return "empty";
    }

    std::ostringstream stream;
    stream << "px=" << level->price
           << " qty=" << level->aggregate_quantity
           << " orders=" << level->order_count;
    return stream.str();
}

inline std::string format_snapshot(const BookSnapshot& snapshot) {
    std::ostringstream stream;
    stream << "best_bid{" << format_level(snapshot.best_bid) << "} "
           << "best_ask{" << format_level(snapshot.best_ask) << "} "
           << "bid_levels=" << snapshot.bid_levels << ' '
           << "ask_levels=" << snapshot.ask_levels << ' '
           << "bid_qty=" << snapshot.total_bid_quantity << ' '
           << "ask_qty=" << snapshot.total_ask_quantity;
    return stream.str();
}

inline std::string format_event(const EngineEvent& event) {
    std::ostringstream stream;
    stream << "event=" << to_string(event.type)
           << " order_id=" << event.order_id
           << " qty=" << event.quantity
           << " price=" << event.price;

    if (!event.reason.empty()) {
        stream << " reason=\"" << event.reason << '"';
    }
    if (event.trade.has_value()) {
        stream << " resting_order_id=" << event.trade->resting_order_id
               << " incoming_order_id=" << event.trade->incoming_order_id
               << " aggressor_side=" << to_string(event.trade->aggressor_side)
               << " trade_qty=" << event.trade->quantity
               << " trade_price=" << event.trade->price
               << " ts=" << event.trade->timestamp;
    }

    return stream.str();
}

}  // namespace lob
