#include "lob/csv_replay.hpp"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    const auto print_book = argc == 3 && std::string_view(argv[1]) == "--print-book";
    if (argc != 2 && !print_book) {
        std::cerr << "usage: lob_replay [--print-book] <csv-file>\n";
        return 1;
    }

    try {
        lob::MatchingEngine engine {10000};
        lob::CsvReplayer replayer;
        const auto result = replayer.replay(print_book ? argv[2] : argv[1], engine);

        std::cout << "commands=" << result.command_count
                  << " trades=" << result.trade_count
                  << " traded_quantity=" << result.traded_quantity
                  << '\n';

        if (print_book) {
            std::cout << lob::format_snapshot(engine.snapshot()) << '\n';
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    return 0;
}
