#include "MovingAverageStrategy.h"
#include <numeric>
#include <sstream>

SignalResult MovingAverageStrategy::onPrice(const std::string& symbol,
                                           double price,
                                           int64_t timestamp) {
    auto& window = windows_[symbol];
    window.push_back(price);
    if ((int)window.size() > longPeriod_) {
        window.pop_front();
    }

    if ((int)window.size() < longPeriod_) {
        return SignalResult();
    }

    // Calculate averages
    auto itShort = window.end() - shortPeriod_;
    double avgShort = std::accumulate(itShort, window.end(), 0.0) / shortPeriod_;
    double avgLong  = std::accumulate(window.begin(), window.end(), 0.0) / longPeriod_;

    // Check for crossover
    bool shortAbove = (avgShort > avgLong);
    
    // Default last state is whatever it is now (no cross on first step)
    if (lastCross_.find(symbol) == lastCross_.end()) {
        lastCross_[symbol] = shortAbove;
        return SignalResult();
    }

    bool prevShortAbove = lastCross_[symbol];
    lastCross_[symbol] = shortAbove;

    if (shortAbove && !prevShortAbove) {
        // Golden Cross -> BUY
        Signal sig;
        sig.symbol   = symbol;
        sig.side     = OrderSide::BUY;
        sig.quantity = tradeQty_;
        sig.price    = 0.0; // Market order
        std::ostringstream oss;
        oss << "Golden Cross (MA" << shortPeriod_ << " > MA" << longPeriod_ << ")";
        sig.reason   = oss.str();
        return SignalResult(sig);
    } 
    else if (!shortAbove && prevShortAbove) {
        // Death Cross -> SELL
        Signal sig;
        sig.symbol   = symbol;
        sig.side     = OrderSide::SELL;
        sig.quantity = tradeQty_;
        sig.price    = 0.0; // Market order
        std::ostringstream oss;
        oss << "Death Cross (MA" << shortPeriod_ << " < MA" << longPeriod_ << ")";
        sig.reason   = oss.str();
        return SignalResult(sig);
    }

    return SignalResult();
}
