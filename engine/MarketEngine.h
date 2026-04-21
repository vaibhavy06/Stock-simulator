#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include "../utils/MinGWThreadFix.h"
#include "../models/Stock.h"
#include "../models/Order.h"
#include "MatchingEngine.h"

// Callback type for strategy engines to receive price updates
using PriceUpdateCallback = std::function<void(const std::string&, double, int64_t)>;

// The MarketEngine drives the simulation:
// - Updates stock prices using random walk + volatility
// - Manages multiple stocks (optionally in parallel)
// - Seeds the order books with synthetic market maker liquidity
class MarketEngine {
public:
    MarketEngine(MatchingEngine& matchingEngine, uint32_t seed = 42);

    // Register a stock for simulation
    void addStock(const std::string& symbol, double initialPrice, double volatility);

    // Register a callback to receive price updates each step
    void onPriceUpdate(PriceUpdateCallback cb) { priceCallbacks_.push_back(std::move(cb)); }

    // Advance simulation by one time step (single-threaded)
    void stepAll(int64_t timestamp);

    // Advance simulation using multiple threads (one per stock)
    void stepAllParallel(int64_t timestamp);

    // Place a synthetic market-maker limit order around current price
    void seedLiquidity(const std::string& symbol, int64_t timestamp);

    const Stock* getStock(const std::string& symbol) const;
    const std::unordered_map<std::string, Stock>& stocks() const { return stocks_; }

    // Build price map for portfolio valuation
    std::unordered_map<std::string, double> currentPrices() const;

private:
    std::unordered_map<std::string, Stock> stocks_;
    MatchingEngine& matchingEngine_;
    std::vector<PriceUpdateCallback> priceCallbacks_;
    std::mt19937 rng_;
    mutable std::mutex mutex_;

    // Apply one random-walk step to a stock
    double randomWalkStep(const Stock& stock);
};
