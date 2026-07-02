#include "lob/csv_replay.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: lob_replay <csv-file>\n";
        return 1;
    }

    try {
        lob::MatchingEngine engine {10000};
        lob::CsvReplayer replayer;
        const auto result = replayer.replay(argv[1], engine);

        std::cout << "commands=" << result.command_count
                  << " trades=" << result.trade_count
                  << " traded_quantity=" << result.traded_quantity
                  << '\n';
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    return 0;
}
