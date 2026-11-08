#include "orderbook.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>

namespace orderbook {

namespace {

std::string trim(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::optional<std::uint64_t> parse_uint64(const std::string& value) {
    std::string trimmed = trim(value);
    if (trimmed.empty()) return std::nullopt;
    std::uint64_t result{};
    const char* first = trimmed.data();
    const char* last = first + trimmed.size();
    const char* parsed_end = first;
    auto parsed = std::from_chars(first, last, result);
    parsed_end = parsed.ptr;
    if (parsed.ec != std::errc{} || parsed_end != last) return std::nullopt;
    return result;
}

std::optional<Price> parse_price(const std::string& value) {
    auto parsed = parse_uint64(value);
    if (!parsed) return std::nullopt;
    return static_cast<Price>(*parsed);
}

OrderId make_order_id(const OrderEvent& event) {
    if (event.order_id) return *event.order_id;
    return event.user_id + "-" + std::to_string(
        std::hash<std::string>{}(event.user_id + "|" + event.stock_code));
}

}

EventResult StockOrderBook::process(const OrderEvent& event) {
    if (event.stock_code != stock_code_) {
        return {Status::Rejected, "stock code does not belong to this order book", {}};
    }
    if (event.user_id.empty()) return {Status::Rejected, "user ID is required", {}};

    OrderId id = make_order_id(event);
    if (event.type == EventType::Cancel) {
        if (!event.order_id) {
            OrderId fallback;
            for (const auto& [candidate_id, candidate] : orders_) {
                if (candidate->user_id == event.user_id &&
                    candidate->side == event.side &&
                    candidate->stock_code == event.stock_code &&
                    (event.price == 0 || candidate->price == event.price)) {
                    fallback = candidate_id;
                    break;
                }
            }
            id = fallback;
        }
        auto order = orders_.find(id);
        if (order == orders_.end()) return {Status::Rejected, "order not found", {}};

        auto& target = *order->second;
        auto erase_from_queue = [&]() -> bool {
            if (target.side == Side::Buy) {
                auto level = bids_.find(target.price);
                if (level == bids_.end()) return false;
                auto& queue = level->second;
                for (auto it = queue.begin(); it != queue.end(); ++it) {
                    if (it->order_id == id) {
                        queue.erase(it);
                        if (queue.empty()) bids_.erase(level);
                        return true;
                    }
                }
            } else {
                auto level = asks_.find(target.price);
                if (level == asks_.end()) return false;
                auto& queue = level->second;
                for (auto it = queue.begin(); it != queue.end(); ++it) {
                    if (it->order_id == id) {
                        queue.erase(it);
                        if (queue.empty()) asks_.erase(level);
                        return true;
                    }
                }
            }
            return false;
        };
        if (!erase_from_queue()) return {Status::Rejected, "order not found in book", {}};
        orders_.erase(id);
        return {Status::Canceled, "order canceled", {}};
    }

    if (event.qty == 0) return {Status::Rejected, "quantity must be greater than zero", {}};
    if (event.price <= 0) return {Status::Rejected, "price must be greater than zero", {}};
    if (!event.order_id) id = event.user_id + "-" + event.stock_code + "-" + std::to_string(next_sequence_);
    if (orders_.contains(id)) return {Status::Rejected, "duplicate order ID", {}};

    Order incoming{id, event.user_id, event.stock_code, event.price, event.qty,
                   event.side, next_sequence_++};
    EventResult result{Status::Accepted, "new order accepted", {}};

    auto consume = [&](Order& resting, Quantity qty) {
        Quantity traded = std::min(resting.remaining_qty, qty);
        resting.remaining_qty -= traded;
        incoming.remaining_qty -= traded;
        result.executions.push_back({
            incoming.side == Side::Buy ? incoming.order_id : resting.order_id,
            incoming.side == Side::Sell ? incoming.order_id : resting.order_id,
            stock_code_,
            resting.price,
            traded
        });
    };

    if (incoming.side == Side::Buy) {
        while (incoming.remaining_qty > 0 && !asks_.empty() && incoming.price >= asks_.begin()->first) {
            auto level_it = asks_.begin();
            auto& queue = level_it->second;
            while (incoming.remaining_qty > 0 && !queue.empty()) {
                Order& resting = queue.front();
                consume(resting, incoming.remaining_qty);
                if (resting.remaining_qty == 0) {
                    orders_.erase(resting.order_id);
                    queue.pop_front();
                }
            }
            if (queue.empty()) asks_.erase(level_it);
        }
    } else {
        while (incoming.remaining_qty > 0 && !bids_.empty() && incoming.price <= bids_.begin()->first) {
            auto level_it = bids_.begin();
            auto& queue = level_it->second;
            while (incoming.remaining_qty > 0 && !queue.empty()) {
                Order& resting = queue.front();
                consume(resting, incoming.remaining_qty);
                if (resting.remaining_qty == 0) {
                    orders_.erase(resting.order_id);
                    queue.pop_front();
                }
            }
            if (queue.empty()) bids_.erase(level_it);
        }
    }

    if (incoming.remaining_qty > 0) {
        auto& queue = incoming.side == Side::Buy
            ? bids_[incoming.price]
            : asks_[incoming.price];
        queue.push_back(incoming);
        orders_[incoming.order_id] = &queue.back();
    } else {
        result.status = Status::Accepted;
    }

    if (!result.executions.empty()) result.message = "new order accepted and executed";
    return result;
}

std::vector<PriceLevel> StockOrderBook::bids() const {
    std::vector<PriceLevel> result;
    for (const auto& [price, queue] : bids_) {
        Quantity total{};
        for (const Order& order : queue) total += order.remaining_qty;
        result.push_back({price, total});
    }
    return result;
}

std::vector<PriceLevel> StockOrderBook::asks() const {
    std::vector<PriceLevel> result;
    for (const auto& [price, queue] : asks_) {
        Quantity total{};
        for (const Order& order : queue) total += order.remaining_qty;
        result.push_back({price, total});
    }
    return result;
}

std::optional<Order> StockOrderBook::get_order(const OrderId& order_id) const {
    auto search = orders_.find(order_id);
    return search == orders_.end() ? std::nullopt : std::optional<Order>(*search->second);
}

EventResult OrderBook::process(const OrderEvent& event) {
    if (event.stock_code.empty()) return {Status::Rejected, "stock code is required", {}};
    return stock(event.stock_code)->process(event);
}

const StockOrderBook* OrderBook::stock(const StockCode& stock_code) const {
    auto found = books_.find(stock_code);
    return found == books_.end() ? nullptr : &found->second;
}

StockOrderBook* OrderBook::stock(const StockCode& stock_code) {
    return &books_.try_emplace(stock_code, stock_code).first->second;
}

std::string to_string(Side side) { return side == Side::Buy ? "Buy" : "Sell"; }
std::string to_string(EventType type) { return type == EventType::New ? "New" : "Cancel"; }
std::string to_string(Status status) {
    switch (status) {
        case Status::Accepted: return "Accepted";
        case Status::Rejected: return "Rejected";
        case Status::Canceled: return "Canceled";
        case Status::Executed: return "Executed";
    }
    return "Rejected";
}

std::optional<Side> parse_side(std::string value) {
    value = trim(value);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (value == "buy" || value == "b") return Side::Buy;
    if (value == "sell" || value == "s") return Side::Sell;
    return std::nullopt;
}

std::optional<EventType> parse_event_type(std::string value) {
    value = trim(value);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (value == "new" || value == "n") return EventType::New;
    if (value == "cancel" || value == "c") return EventType::Cancel;
    return std::nullopt;
}

}

namespace {
std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) fields.push_back(field);
    return fields;
}

void print_book(const orderbook::StockOrderBook& book) {
    std::cout << "Bids:\n";
    for (const auto& level : book.bids()) std::cout << "  " << level.price << " " << level.qty << '\n';
    std::cout << "Asks:\n";
    for (const auto& level : book.asks()) std::cout << "  " << level.price << " " << level.qty << '\n';
}
}

int main(int argc, char** argv) {
    orderbook::OrderBook engine;
    const std::string selected_stock = argc > 1 ? argv[1] : "";

    if (argc == 1) {
        std::cout << "Usage: orderbook STOCK_CODE\n";
        std::cout << "CSV input: user_id,stock_code,price,qty,side,new/cancel,order_id(optional)\n";
        std::cout << "Example: u1,AAPL,101,10,Buy,New,o1\n";
        return 0;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto fields = split_csv(line);
        if (fields.size() < 6) {
            std::cout << "Rejected,invalid CSV input\n";
            continue;
        }

        auto side = orderbook::parse_side(fields[4]);
        auto type = orderbook::parse_event_type(fields[5]);
        auto qty = orderbook::parse_uint64(fields[3]);
        auto price = orderbook::parse_price(fields[2]);
        if (!side || !type || !qty || !price) {
            std::cout << "Rejected,invalid event fields\n";
            continue;
        }

        orderbook::OrderEvent event{
            orderbook::trim(fields[0]),
            orderbook::trim(fields[1]),
            *price,
            *qty,
            *side,
            *type,
            fields.size() > 6 ? std::optional<orderbook::OrderId>(orderbook::trim(fields[6])) : std::nullopt
        };

        if (!selected_stock.empty() && event.stock_code != selected_stock) {
            std::cout << "Rejected,stock code does not match selected stock\n";
            continue;
        }

        auto result = engine.process(event);
        std::cout << orderbook::to_string(result.status) << ',' << result.message << '\n';
        for (const auto& execution : result.executions) {
            std::cout << "Executed," << execution.stock_code << ',' << execution.price << ','
                      << execution.qty << ',' << execution.buy_order_id << ',' << execution.sell_order_id << '\n';
        }
        print_book(*engine.stock(selected_stock));
    }
    return 0;
}