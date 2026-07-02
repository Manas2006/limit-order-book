#pragma once

#include "lob/matching_engine.hpp"

#include <filesystem>
#include <vector>

namespace lob {

struct ReplayResult {
    std::size_t command_count {};
    std::size_t trade_count {};
    Quantity traded_quantity {};
    std::vector<EngineEvent> events;
};

class CsvReplayer {
public:
    [[nodiscard]] std::vector<ReplayCommand> load(const std::filesystem::path& path) const;
    [[nodiscard]] ReplayResult replay(const std::filesystem::path& path, MatchingEngine& engine) const;
};

}  // namespace lob
