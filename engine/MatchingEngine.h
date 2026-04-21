#pragma once
#include <string>
#include <map>
#include <deque>
#include <vector>
#include <mutex>
#include <functional>
#include "../models/Order.h"
#include "../models/Trade.h"

// Order book for one side (bid or ask)
// Bids: sorted descending by price (highest bid first)
// Asks: sorted ascending by price  (lowest ask first)
using BidBook = std::map<double, std::deque<Order>, std::greater<double>>;
using AskBook = std::map<double, std::deque<Order>>;

class MatchingEngine {
public:
    // Callback invoked each time a trade is generated
    using TradeCallback = std::function<void(const Trade&)>;

    explicit MatchingEngine(TradeCallback cb = nullptr)
        : tradeCallback_(std::move(cb)) {}

    // Submit an order; may immediately match and generate trades
    void submitOrder(Order order, int64_t currentTimestamp);

    // Cancel an existing order by id
    bool cancelOrder(uint64_t orderId, const std::string& symbol, OrderSide side);

    // Access order books (read-only)
    const BidBook& bidBook(const std::string& symbol) const;
    const AskBook& askBook(const std::string& symbol) const;

    // Best bid/ask prices
    double bestBid(const std::string& symbol) const;
    double bestAsk(const std::string& symbol) const;

    // All trades so far
    const std::vector<Trade>& trades() const { return trades_; }

    void setTradeCallback(TradeCallback cb) { tradeCallback_ = std::move(cb); }

private:
    std::unordered_map<std::string, BidBook> bidBooks_;
    std::unordered_map<std::string, AskBook> askBooks_;
    std::vector<Trade> trades_;
    TradeCallback tradeCallback_;
    mutable std::mutex mutex_;

    // Core matching logic
    void matchOrder(Order& incoming, int64_t ts);
    void addToBook(const Order& order);
    void removeFilledFromBook(const std::string& symbol, OrderSide side);

    Trade makeTrade(const std::string& symbol, double price, int qty,
                    uint64_t buyId, uint64_t sellId, int64_t ts);
};
