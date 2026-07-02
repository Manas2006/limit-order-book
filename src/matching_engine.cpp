#include "lob/matching_engine.hpp"

#include <stdexcept>

namespace lob {

MatchingEngine::MatchingEngine(std::size_t reserve_orders) {
    orders_.reserve(reserve_orders);
}

void MatchingEngine::set_event_callback(EventCallback callback) {
    callback_ = std::move(callback);
}

bool MatchingEngine::submit(const OrderRequest& request) {
    if (request.quantity == 0) {
        reject(request.order_id, request.quantity, request.price, "quantity must be positive");
        return false;
    }
    if (request.type == OrderType::limit && request.price <= 0) {
        reject(request.order_id, request.quantity, request.price, "limit price must be positive");
        return false;
    }
    if (orders_.contains(request.order_id)) {
        reject(request.order_id, request.quantity, request.price, "duplicate order id");
        return false;
    }

    auto& incoming = allocate_order(request);
    emit(EngineEvent {
        .type = EventType::accepted,
        .order_id = incoming.order_id,
        .quantity = incoming.remaining,
        .price = incoming.price,
    });

    match(incoming);

    if (incoming.remaining == 0) {
        incoming.active = false;
        orders_.erase(incoming.order_id);
        return true;
    }

    if (incoming.type == OrderType::market) {
        reject(incoming.order_id, incoming.remaining, incoming.price, "market order not fully filled");
        incoming.active = false;
        orders_.erase(incoming.order_id);
        return true;
    }

    add_to_book(incoming);
    return true;
}

bool MatchingEngine::cancel(const CancelRequest& request) {
    auto it = orders_.find(request.order_id);
    if (it == orders_.end() || !it->second->active) {
        reject(request.order_id, 0, 0, "cancel target not found");
        return false;
    }

    auto& node = *it->second;
    const auto remaining = node.remaining;
    const auto price = node.price;
    remove_from_book(node);
    node.active = false;
    orders_.erase(it);

    emit(EngineEvent {
        .type = EventType::canceled,
        .order_id = request.order_id,
        .quantity = remaining,
        .price = price,
    });

    return true;
}

bool MatchingEngine::modify(const ModifyRequest& request) {
    auto it = orders_.find(request.order_id);
    if (it == orders_.end() || !it->second->active) {
        reject(request.order_id, request.new_quantity, request.new_price, "modify target not found");
        return false;
    }
    if (request.new_quantity == 0) {
        reject(request.order_id, request.new_quantity, request.new_price, "new quantity must be positive");
        return false;
    }
    if (request.new_price <= 0) {
        reject(request.order_id, request.new_quantity, request.new_price, "new price must be positive");
        return false;
    }

    auto& existing = *it->second;
    const auto original_type = existing.type;
    const auto side = existing.side;
    remove_from_book(existing);

    existing.price = request.new_price;
    existing.remaining = request.new_quantity;
    existing.timestamp = request.timestamp;
    existing.type = OrderType::limit;
    existing.prev = nullptr;
    existing.next = nullptr;

    emit(EngineEvent {
        .type = EventType::modified,
        .order_id = existing.order_id,
        .quantity = existing.remaining,
        .price = existing.price,
    });

    match(existing);
    if (existing.remaining == 0) {
        existing.active = false;
        orders_.erase(it);
        return true;
    }

    existing.side = side;
    existing.type = original_type == OrderType::market ? OrderType::limit : original_type;
    add_to_book(existing);
    return true;
}

std::optional<BookLevel> MatchingEngine::best_bid() const {
    return best_level(bids_);
}

std::optional<BookLevel> MatchingEngine::best_ask() const {
    return best_level(asks_);
}

std::vector<BookLevel> MatchingEngine::bids() const {
    return snapshot(bids_);
}

std::vector<BookLevel> MatchingEngine::asks() const {
    return snapshot(asks_);
}

bool MatchingEngine::has_order(OrderId order_id) const {
    const auto it = orders_.find(order_id);
    return it != orders_.end() && it->second->active;
}

bool MatchingEngine::match(OrderNode& incoming) {
    auto matched = false;
    const auto consume_level = [&](auto& contra_book) {
        while (incoming.remaining > 0 && !contra_book.empty()) {
            auto level_it = contra_book.begin();
            if (!crosses(incoming, level_it->first)) {
                break;
            }

            auto& level = level_it->second;
            auto* resting = level.head;
            while (incoming.remaining > 0 && resting != nullptr) {
                matched = true;
                auto* next = resting->next;
                const auto fill = incoming.remaining < resting->remaining ? incoming.remaining : resting->remaining;

                incoming.remaining -= fill;
                resting->remaining -= fill;
                level.aggregate_quantity -= fill;

                emit(EngineEvent {
                    .type = EventType::trade,
                    .order_id = incoming.order_id,
                    .quantity = fill,
                    .price = resting->price,
                    .trade = TradeEvent {
                        .resting_order_id = resting->order_id,
                        .incoming_order_id = incoming.order_id,
                        .aggressor_side = incoming.side,
                        .price = resting->price,
                        .quantity = fill,
                        .timestamp = incoming.timestamp,
                    },
                });

                if (resting->remaining == 0) {
                    remove_from_book(*resting);
                    resting->active = false;
                    orders_.erase(resting->order_id);
                }

                resting = next;
            }
        }
    };

    if (incoming.side == Side::buy) {
        consume_level(asks_);
    } else {
        consume_level(bids_);
    }

    return matched;
}

bool MatchingEngine::crosses(OrderNode& incoming, Price best_price) const {
    if (incoming.type == OrderType::market) {
        return true;
    }
    if (incoming.side == Side::buy) {
        return incoming.price >= best_price;
    }
    return incoming.price <= best_price;
}

void MatchingEngine::add_to_book(OrderNode& node) {
    auto& level = find_or_create_level(node.side, node.price);
    node.prev = level.tail;
    node.next = nullptr;
    if (level.tail != nullptr) {
        level.tail->next = &node;
    } else {
        level.head = &node;
    }
    level.tail = &node;
    level.aggregate_quantity += node.remaining;
    ++level.order_count;
}

void MatchingEngine::remove_from_book(OrderNode& node) {
    auto* level_ptr = [&]() -> PriceLevel* {
        if (node.side == Side::buy) {
            auto it = bids_.find(node.price);
            return it == bids_.end() ? nullptr : &it->second;
        }
        auto it = asks_.find(node.price);
        return it == asks_.end() ? nullptr : &it->second;
    }();

    if (level_ptr == nullptr) {
        return;
    }

    auto& level = *level_ptr;
    if (node.prev != nullptr) {
        node.prev->next = node.next;
    } else {
        level.head = node.next;
    }
    if (node.next != nullptr) {
        node.next->prev = node.prev;
    } else {
        level.tail = node.prev;
    }

    level.aggregate_quantity -= node.remaining;
    --level.order_count;
    node.prev = nullptr;
    node.next = nullptr;

    remove_if_empty(node.side, node.price);
}

void MatchingEngine::remove_if_empty(Side side, Price price) {
    if (side == Side::buy) {
        auto it = bids_.find(price);
        if (it != bids_.end() && it->second.order_count == 0) {
            bids_.erase(it);
        }
        return;
    }

    auto it = asks_.find(price);
    if (it != asks_.end() && it->second.order_count == 0) {
        asks_.erase(it);
    }
}

void MatchingEngine::emit(const EngineEvent& event) const {
    if (callback_) {
        callback_(event);
    }
}

void MatchingEngine::reject(OrderId order_id, Quantity quantity, Price price, const char* reason) const {
    emit(EngineEvent {
        .type = EventType::rejected,
        .order_id = order_id,
        .quantity = quantity,
        .price = price,
        .reason = reason,
    });
}

MatchingEngine::OrderNode& MatchingEngine::allocate_order(const OrderRequest& request) {
    order_storage_.push_back(OrderNode {
        .order_id = request.order_id,
        .side = request.side,
        .type = request.type,
        .price = request.price,
        .remaining = request.quantity,
        .timestamp = request.timestamp,
        .active = true,
    });

    auto& node = order_storage_.back();
    orders_.emplace(node.order_id, &node);
    return node;
}

MatchingEngine::PriceLevel& MatchingEngine::find_or_create_level(Side side, Price price) {
    if (side == Side::buy) {
        auto [it, _] = bids_.try_emplace(price, PriceLevel {.price = price});
        return it->second;
    }
    auto [it, _] = asks_.try_emplace(price, PriceLevel {.price = price});
    return it->second;
}

std::optional<BookLevel> MatchingEngine::best_level(const BidBook& book) const {
    if (book.empty()) {
        return std::nullopt;
    }
    const auto& level = book.begin()->second;
    return BookLevel {
        .price = level.price,
        .aggregate_quantity = level.aggregate_quantity,
        .order_count = level.order_count,
    };
}

std::optional<BookLevel> MatchingEngine::best_level(const AskBook& book) const {
    if (book.empty()) {
        return std::nullopt;
    }
    const auto& level = book.begin()->second;
    return BookLevel {
        .price = level.price,
        .aggregate_quantity = level.aggregate_quantity,
        .order_count = level.order_count,
    };
}

std::vector<BookLevel> MatchingEngine::snapshot(const BidBook& book) const {
    std::vector<BookLevel> levels;
    levels.reserve(book.size());
    for (const auto& [_, level] : book) {
        levels.push_back(BookLevel {
            .price = level.price,
            .aggregate_quantity = level.aggregate_quantity,
            .order_count = level.order_count,
        });
    }
    return levels;
}

std::vector<BookLevel> MatchingEngine::snapshot(const AskBook& book) const {
    std::vector<BookLevel> levels;
    levels.reserve(book.size());
    for (const auto& [_, level] : book) {
        levels.push_back(BookLevel {
            .price = level.price,
            .aggregate_quantity = level.aggregate_quantity,
            .order_count = level.order_count,
        });
    }
    return levels;
}

}  // namespace lob
