#include "lob/csv_replay.hpp"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    bool print_book = false;
    bool print_events = false;
    const char* csv_path = nullptr;

    for (int i = 1; i < argc; ++i) {
        const auto arg = std::string_view(argv[i]);
        if (arg == "--print-book") {
            print_book = true;
            continue;
        }
        if (arg == "--print-events") {
            print_events = true;
            continue;
        }
        if (csv_path == nullptr) {
            csv_path = argv[i];
            continue;
        }
        csv_path = nullptr;
        break;
    }

    if (csv_path == nullptr) {
        std::cerr << "usage: lob_replay [--print-book] [--print-events] <csv-file>\n";
        return 1;
    }

    try {
        lob::MatchingEngine engine {10000};
        lob::CsvReplayer replayer;
        const auto result = replayer.replay(csv_path, engine);

        std::cout << "commands=" << result.command_count
                  << " accepted=" << result.accepted_count
                  << " canceled=" << result.canceled_count
                  << " modified=" << result.modified_count
                  << " rejected=" << result.rejected_count
                  << " trades=" << result.trade_count
                  << " traded_quantity=" << result.traded_quantity
                  << " traded_notional=" << result.traded_notional
                  << '\n';

        if (print_events) {
            for (const auto& event : result.events) {
                std::cout << lob::format_event(event) << '\n';
            }
        }
        if (print_book) {
            std::cout << lob::format_snapshot(engine.snapshot()) << '\n';
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    return 0;
}
