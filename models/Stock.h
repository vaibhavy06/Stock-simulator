#pragma once
#include <string>
#include <vector>
#include <deque>
#include <numeric>
#include <algorithm>
#include <cmath>

// Represents a single stock in the simulation
struct Stock {
    std::string symbol;
    double currentPrice;
    double volatility;       // Controls random walk magnitude (e.g., 0.02 = 2%)
    double openPrice;        // Price at start of session
    double highPrice;
    double lowPrice;
    std::deque<double> priceHistory;  // Rolling historical prices
    static constexpr size_t MAX_HISTORY = 500;

    Stock() = default;

    Stock(const std::string& sym, double price, double vol)
        : symbol(sym), currentPrice(price), volatility(vol),
          openPrice(price), highPrice(price), lowPrice(price) {
        priceHistory.push_back(price);
    }

    // Update price and maintain high/low/history
    void updatePrice(double newPrice) {
        currentPrice = newPrice;
        highPrice = std::max(highPrice, newPrice);
        lowPrice  = std::min(lowPrice,  newPrice);
        priceHistory.push_back(newPrice);
        if (priceHistory.size() > MAX_HISTORY)
            priceHistory.pop_front();
    }

    // Simple moving average over last N periods
    double sma(size_t periods) const {
        if (priceHistory.size() < periods) return currentPrice;
        double sum = 0.0;
        auto it = priceHistory.end();
        for (size_t i = 0; i < periods; ++i) sum += *(--it);
        return sum / static_cast<double>(periods);
    }

    // Daily return percentage
    double dailyReturn() const {
        if (openPrice == 0.0) return 0.0;
        return (currentPrice - openPrice) / openPrice * 100.0;
    }
};
