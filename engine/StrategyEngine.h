#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "../strategies/Strategy.h"
#include "../models/Portfolio.h"
#include "MatchingEngine.h"
#include "../utils/Logger.h"

// Orchestrates strategy signals -> orders -> matching engine -> portfolio updates
class StrategyEngine {
public:
    StrategyEngine(MatchingEngine& me, Portfolio& portfolio)
        : matchingEngine_(me), portfolio_(portfolio) {
        // Register trade callback to update portfolio on execution
        matchingEngine_.setTradeCallback([this](const Trade& t) {
            onTrade(t);
        });
    }

    // Register a strategy (can register multiple)
    void addStrategy(std::shared_ptr<Strategy> strategy) {
        strategies_.push_back(std::move(strategy));
    }

    // Feed a new price tick to all strategies; act on signals
    void onPrice(const std::string& symbol, double price, int64_t timestamp);

    // Track which side we last traded per symbol for portfolio updates
    void recordPendingOrder(uint64_t orderId, OrderSide side, const std::string& symbol);

private:
    MatchingEngine& matchingEngine_;
    Portfolio& portfolio_;
    std::vector<std::shared_ptr<Strategy>> strategies_;

    // Map order id -> (side, symbol) so portfolio can be updated when trade fires
    std::unordered_map<uint64_t, std::pair<OrderSide, std::string>> pendingOrders_;
    std::mutex mutex_;

    void onTrade(const Trade& trade);
    bool canBuy(const std::string& symbol, int qty, double price) const;
    bool canSell(const std::string& symbol, int qty) const;
};
