#include "MarketEngine.h"
#include <cmath>
#include <sstream>

MarketEngine::MarketEngine(MatchingEngine& me, uint32_t seed)
    : matchingEngine_(me), rng_(seed) {}

void MarketEngine::addStock(const std::string& symbol,
                              double initialPrice, double volatility) {
    stocks_.emplace(symbol, Stock(symbol, initialPrice, volatility));
}

void MarketEngine::stepAll(int64_t timestamp) {
    for (auto it = stocks_.begin(); it != stocks_.end(); ++it) {
        const std::string& sym = it->first;
        Stock& stock = it->second;
        double newPrice = randomWalkStep(stock);
        stock.updatePrice(newPrice);
        seedLiquidity(sym, timestamp);
        for (auto& cb : priceCallbacks_) cb(sym, newPrice, timestamp);
    }
}

void MarketEngine::stepAllParallel(int64_t timestamp) {
    // Each stock gets its own thread for price update + liquidity seeding
    std::vector<std::thread> threads;
    threads.reserve(stocks_.size());

    for (auto it = stocks_.begin(); it != stocks_.end(); ++it) {
        const std::string* pSym = &(it->first);
        Stock* pStock = &(it->second);
        threads.emplace_back([this, pSym, pStock, timestamp]() {
            double newPrice;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                newPrice = randomWalkStep(*pStock);
                pStock->updatePrice(newPrice);
            }
            seedLiquidity(*pSym, timestamp);
            for (auto& cb : priceCallbacks_) cb(*pSym, newPrice, timestamp);
        });
    }

    for (auto& t : threads) t.join();
}

void MarketEngine::seedLiquidity(const std::string& symbol, int64_t timestamp) {
    auto it = stocks_.find(symbol);
    if (it == stocks_.end()) return;

    double midPrice = it->second.currentPrice;
    double spread   = midPrice * 0.001; // 0.1% spread

    // Place a resting ask slightly above mid and a resting bid slightly below
    Order ask(symbol, OrderType::LIMIT, OrderSide::SELL,
               midPrice + spread, 50, timestamp, "MarketMaker");
    Order bid(symbol, OrderType::LIMIT, OrderSide::BUY,
               midPrice - spread, 50, timestamp, "MarketMaker");

    matchingEngine_.submitOrder(std::move(ask), timestamp);
    matchingEngine_.submitOrder(std::move(bid), timestamp);
}

double MarketEngine::randomWalkStep(const Stock& stock) {
    // Geometric Brownian Motion: dS = S * (mu*dt + sigma*dW)
    // mu=0 (no drift for simplicity), sigma=volatility, dW~N(0,1)
    std::normal_distribution<double> dist(0.0, 1.0);
    double dW = dist(rng_);
    double pct = stock.volatility * dW;        // daily return shock
    double newPrice = stock.currentPrice * std::exp(pct);
    // Hard floor: price can't go below $0.01
    return std::max(newPrice, 0.01);
}

const Stock* MarketEngine::getStock(const std::string& symbol) const {
    auto it = stocks_.find(symbol);
    return it != stocks_.end() ? &it->second : nullptr;
}

std::unordered_map<std::string, double> MarketEngine::currentPrices() const {
    std::unordered_map<std::string, double> prices;
    for (auto it = stocks_.begin(); it != stocks_.end(); ++it) {
        prices[it->first] = it->second.currentPrice;
    }
    return prices;
}
