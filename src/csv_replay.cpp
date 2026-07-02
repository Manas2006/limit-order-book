#include "lob/csv_replay.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lob {

namespace {

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

Side parse_side(std::string_view side) {
    if (side == "buy") {
        return Side::buy;
    }
    if (side == "sell") {
        return Side::sell;
    }
    throw std::runtime_error("invalid side");
}

OrderType parse_order_type(std::string_view type) {
    if (type == "limit") {
        return OrderType::limit;
    }
    if (type == "market") {
        return OrderType::market;
    }
    throw std::runtime_error("invalid order type");
}

CommandType parse_command_type(std::string_view type) {
    if (type == "add") {
        return CommandType::add;
    }
    if (type == "cancel") {
        return CommandType::cancel;
    }
    if (type == "modify") {
        return CommandType::modify;
    }
    throw std::runtime_error("invalid command type");
}

std::uint64_t parse_u64(std::string_view value) {
    return static_cast<std::uint64_t>(std::stoull(std::string(value)));
}

std::int64_t parse_i64(std::string_view value) {
    return static_cast<std::int64_t>(std::stoll(std::string(value)));
}

}  // namespace

std::vector<ReplayCommand> CsvReplayer::load(const std::filesystem::path& path) const {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open CSV file: " + path.string());
    }

    std::vector<ReplayCommand> commands;
    std::string line;
    std::getline(input, line);

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        const auto fields = split_csv_line(line);
        if (fields.size() < 8) {
            throw std::runtime_error("expected at least 8 CSV fields");
        }

        const auto kind = parse_command_type(fields[1]);
        ReplayCommand command {.type = kind};

        if (kind == CommandType::add) {
            command.add = OrderRequest {
                .order_id = parse_u64(fields[2]),
                .side = parse_side(fields[3]),
                .type = parse_order_type(fields[4]),
                .price = parse_i64(fields[5]),
                .quantity = parse_u64(fields[6]),
                .timestamp = parse_u64(fields[0]),
            };
        } else if (kind == CommandType::cancel) {
            command.cancel = CancelRequest {
                .order_id = parse_u64(fields[2]),
                .timestamp = parse_u64(fields[0]),
            };
        } else {
            command.modify = ModifyRequest {
                .order_id = parse_u64(fields[2]),
                .new_price = parse_i64(fields[7]),
                .new_quantity = parse_u64(fields.size() > 8 ? fields[8] : fields[6]),
                .timestamp = parse_u64(fields[0]),
            };
        }

        commands.push_back(command);
    }

    return commands;
}

ReplayResult CsvReplayer::replay(const std::filesystem::path& path, MatchingEngine& engine) const {
    ReplayResult result;
    engine.set_event_callback([&result](const EngineEvent& event) {
        result.events.push_back(event);
        if (event.type == EventType::trade && event.trade.has_value()) {
            ++result.trade_count;
            result.traded_quantity += event.trade->quantity;
        }
    });

    for (const auto& command : load(path)) {
        ++result.command_count;
        switch (command.type) {
        case CommandType::add:
            engine.submit(*command.add);
            break;
        case CommandType::cancel:
            engine.cancel(*command.cancel);
            break;
        case CommandType::modify:
            engine.modify(*command.modify);
            break;
        }
    }

    return result;
}

}  // namespace lob
