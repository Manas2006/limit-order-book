#include "lob/csv_replay.hpp"
#include "lob/matching_engine.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("price time priority is preserved across partial fills") {
    lob::MatchingEngine engine;
    std::vector<lob::TradeEvent> trades;
    engine.set_event_callback([&](const lob::EngineEvent& event) {
        if (event.type == lob::EventType::trade && event.trade.has_value()) {
            trades.push_back(*event.trade);
        }
    });

    REQUIRE(engine.submit({1, lob::Side::sell, lob::OrderType::limit, 101, 50, 1}));
    REQUIRE(engine.submit({2, lob::Side::sell, lob::OrderType::limit, 101, 75, 2}));
    REQUIRE(engine.submit({3, lob::Side::buy, lob::OrderType::limit, 101, 100, 3}));

    REQUIRE(trades.size() == 2);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 50);
    CHECK(trades[1].resting_order_id == 2);
    CHECK(trades[1].quantity == 50);

    const auto ask = engine.best_ask();
    REQUIRE(ask.has_value());
    CHECK(ask->price == 101);
    CHECK(ask->aggregate_quantity == 25);
}

TEST_CASE("market orders consume available liquidity and do not rest") {
    lob::MatchingEngine engine;
    REQUIRE(engine.submit({10, lob::Side::sell, lob::OrderType::limit, 102, 40, 1}));
    REQUIRE(engine.submit({11, lob::Side::buy, lob::OrderType::market, 0, 100, 2}));

    CHECK_FALSE(engine.best_ask().has_value());
    CHECK_FALSE(engine.has_order(11));
}

TEST_CASE("cancel removes a resting order") {
    lob::MatchingEngine engine;
    REQUIRE(engine.submit({20, lob::Side::buy, lob::OrderType::limit, 99, 25, 1}));
    REQUIRE(engine.cancel({20, 2}));
    CHECK_FALSE(engine.has_order(20));
    CHECK_FALSE(engine.best_bid().has_value());
}

TEST_CASE("modify re-prices order and preserves deterministic matching") {
    lob::MatchingEngine engine;
    REQUIRE(engine.submit({30, lob::Side::buy, lob::OrderType::limit, 100, 50, 1}));
    REQUIRE(engine.submit({31, lob::Side::sell, lob::OrderType::limit, 105, 20, 2}));
    REQUIRE(engine.modify({30, 106, 50, 3}));

    CHECK_FALSE(engine.has_order(31));
    const auto bid = engine.best_bid();
    REQUIRE(bid.has_value());
    CHECK(bid->price == 106);
    CHECK(bid->aggregate_quantity == 30);
}

TEST_CASE("same price quantity reduction preserves queue priority") {
    lob::MatchingEngine engine;
    std::vector<lob::TradeEvent> trades;
    engine.set_event_callback([&](const lob::EngineEvent& event) {
        if (event.type == lob::EventType::trade && event.trade.has_value()) {
            trades.push_back(*event.trade);
        }
    });

    REQUIRE(engine.submit({50, lob::Side::sell, lob::OrderType::limit, 101, 40, 1}));
    REQUIRE(engine.submit({51, lob::Side::sell, lob::OrderType::limit, 101, 40, 2}));
    REQUIRE(engine.modify({50, 101, 25, 3}));
    REQUIRE(engine.submit({52, lob::Side::buy, lob::OrderType::limit, 101, 30, 4}));

    REQUIRE(trades.size() == 2);
    CHECK(trades[0].resting_order_id == 50);
    CHECK(trades[0].quantity == 25);
    CHECK(trades[1].resting_order_id == 51);
    CHECK(trades[1].quantity == 5);
}

TEST_CASE("same price quantity increase resets priority by requeueing order") {
    lob::MatchingEngine engine;
    std::vector<lob::TradeEvent> trades;
    engine.set_event_callback([&](const lob::EngineEvent& event) {
        if (event.type == lob::EventType::trade && event.trade.has_value()) {
            trades.push_back(*event.trade);
        }
    });

    REQUIRE(engine.submit({60, lob::Side::sell, lob::OrderType::limit, 101, 20, 1}));
    REQUIRE(engine.submit({61, lob::Side::sell, lob::OrderType::limit, 101, 20, 2}));
    REQUIRE(engine.modify({60, 101, 30, 3}));
    REQUIRE(engine.submit({62, lob::Side::buy, lob::OrderType::limit, 101, 25, 4}));

    REQUIRE(trades.size() == 2);
    CHECK(trades[0].resting_order_id == 61);
    CHECK(trades[0].quantity == 20);
    CHECK(trades[1].resting_order_id == 60);
    CHECK(trades[1].quantity == 5);
}

TEST_CASE("csv replay executes deterministically") {
    lob::MatchingEngine engine;
    lob::CsvReplayer replayer;
    const auto result = replayer.replay("data/sample_orders.csv", engine);

    CHECK(result.command_count == 6);
    CHECK(result.accepted_count == 4);
    CHECK(result.canceled_count == 1);
    CHECK(result.modified_count == 1);
    CHECK(result.rejected_count == 1);
    CHECK(result.trade_count == 2);
    CHECK(result.traded_quantity == 80);
    CHECK(result.traded_notional == 8120);
}

TEST_CASE("engine snapshot summarizes visible book state") {
    lob::MatchingEngine engine;
    REQUIRE(engine.submit({41, lob::Side::buy, lob::OrderType::limit, 100, 25, 1}));
    REQUIRE(engine.submit({42, lob::Side::buy, lob::OrderType::limit, 99, 10, 2}));
    REQUIRE(engine.submit({43, lob::Side::sell, lob::OrderType::limit, 103, 40, 3}));

    const auto snapshot = engine.snapshot();
    REQUIRE(snapshot.best_bid.has_value());
    REQUIRE(snapshot.best_ask.has_value());
    CHECK(snapshot.best_bid->price == 100);
    CHECK(snapshot.best_ask->price == 103);
    CHECK(snapshot.bid_levels == 2);
    CHECK(snapshot.ask_levels == 1);
    CHECK(snapshot.total_bid_quantity == 35);
    CHECK(snapshot.total_ask_quantity == 40);
    CHECK(lob::format_snapshot(snapshot).find("best_bid{px=100 qty=25 orders=1}") != std::string::npos);
}
