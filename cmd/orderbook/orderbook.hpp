#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

namespace orderbook {

using Price = std::int64_t;
using Quantity = std::uint64_t;
using OrderId = std::string;
using UserId = std::string;
using StockCode = std::string;

enum class Side { Buy, Sell };
enum class EventType { New, Cancel };
enum class Status { Accepted, Rejected, Canceled, Executed };

struct OrderEvent {
    UserId user_id;
    StockCode stock_code;
    Price price{};
    Quantity qty{};
    Side side{Side::Buy};
    EventType type{EventType::New};
    std::optional<OrderId> order_id;
};

struct Order {
    OrderId order_id;
    UserId user_id;
    StockCode stock_code;
    Price price;
    Quantity remaining_qty;
    Side side;
    std::uint64_t arrival_sequence;
};

struct Execution {
    OrderId buy_order_id;
    OrderId sell_order_id;
    StockCode stock_code;
    Price price;
    Quantity qty;
};

struct EventResult {
    Status status;
    std::string message;
    std::vector<Execution> executions;
};

struct PriceLevel {
    Price price;
    Quantity qty;
};

class StockOrderBook {
public:
    explicit StockOrderBook(StockCode stock_code) : stock_code_(std::move(stock_code)) {}

    EventResult process(const OrderEvent& event);

    std::vector<PriceLevel> bids() const;
    std::vector<PriceLevel> asks() const;
    std::optional<Order> get_order(const OrderId& order_id) const;

private:
    StockCode stock_code_;
    std::uint64_t next_sequence_{1};
    std::map<Price, std::deque<Order>, std::greater<Price>> bids_;
    std::map<Price, std::deque<Order>> asks_;
    std::unordered_map<OrderId, Order*> orders_;
};

class OrderBook {
public:
    EventResult process(const OrderEvent& event);
    const StockOrderBook* stock(const StockCode& stock_code) const;
    StockOrderBook* stock(const StockCode& stock_code);

private:
    std::map<StockCode, StockOrderBook> books_;
};

std::string to_string(Side side);
std::string to_string(EventType type);
std::string to_string(Status status);
std::optional<Side> parse_side(std::string value);
std::optional<EventType> parse_event_type(std::string value);

}
