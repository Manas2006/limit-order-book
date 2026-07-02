#pragma once

#include "lob/types.hpp"

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace lob {

class MatchingEngine {
public:
    using EventCallback = std::function<void(const EngineEvent&)>;

    explicit MatchingEngine(std::size_t reserve_orders = 0);

    void set_event_callback(EventCallback callback);

    bool submit(const OrderRequest& request);
    bool cancel(const CancelRequest& request);
    bool modify(const ModifyRequest& request);

    [[nodiscard]] std::optional<BookLevel> best_bid() const;
    [[nodiscard]] std::optional<BookLevel> best_ask() const;
    [[nodiscard]] std::vector<BookLevel> bids() const;
    [[nodiscard]] std::vector<BookLevel> asks() const;
    [[nodiscard]] BookSnapshot snapshot() const;
    [[nodiscard]] bool has_order(OrderId order_id) const;

private:
    struct OrderNode {
        OrderId order_id {};
        Side side {};
        OrderType type {};
        Price price {};
        Quantity remaining {};
        Timestamp timestamp {};
        bool active {false};
        OrderNode* prev {nullptr};
        OrderNode* next {nullptr};
    };

    struct PriceLevel {
        Price price {};
        Quantity aggregate_quantity {};
        std::size_t order_count {};
        OrderNode* head {nullptr};
        OrderNode* tail {nullptr};
    };

    using BidBook = std::map<Price, PriceLevel, std::greater<>>;
    using AskBook = std::map<Price, PriceLevel, std::less<>>;

    bool match(OrderNode& incoming);
    bool crosses(OrderNode& incoming, Price best_price) const;
    void add_to_book(OrderNode& node);
    void remove_from_book(OrderNode& node);
    void remove_if_empty(Side side, Price price);
    void emit(const EngineEvent& event) const;
    void reject(OrderId order_id, Quantity quantity, Price price, const char* reason) const;
    OrderNode& allocate_order(const OrderRequest& request);
    PriceLevel& find_or_create_level(Side side, Price price);
    PriceLevel* find_level(Side side, Price price);
    std::optional<BookLevel> best_level(const BidBook& book) const;
    std::optional<BookLevel> best_level(const AskBook& book) const;
    std::vector<BookLevel> snapshot(const BidBook& book) const;
    std::vector<BookLevel> snapshot(const AskBook& book) const;

    std::deque<OrderNode> order_storage_;
    std::unordered_map<OrderId, OrderNode*> orders_;
    BidBook bids_;
    AskBook asks_;
    EventCallback callback_;
};

}  // namespace lob
