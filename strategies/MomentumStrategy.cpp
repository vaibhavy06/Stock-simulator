#include "MomentumStrategy.h"
#include <sstream>

SignalResult MomentumStrategy::onPrice(const std::string& symbol,
                                    double price,
                                    int64_t timestamp) {
    auto& history = priceHistory_[symbol];
    history.push_back(price);
    
    if ((int)history.size() > lookback_) {
        history.pop_front();
    }

    if (cooldownCounter_[symbol] > 0) {
        cooldownCounter_[symbol]--;
        return SignalResult();
    }

    if ((int)history.size() < lookback_) {
        return SignalResult();
    }

    double oldPrice = history.front();
    double returns  = (price - oldPrice) / oldPrice;

    if (returns > threshold_) {
        // Strong positive momentum -> BUY
        Signal sig;
        sig.symbol   = symbol;
        sig.side     = OrderSide::BUY;
        sig.quantity = tradeQty_;
        sig.price    = 0.0;
        std::ostringstream oss;
        oss << "Positive Momentum (" << (returns * 100.0) << "%)";
        sig.reason   = oss.str();
        cooldownCounter_[symbol] = cooldown_;
        return SignalResult(sig);
    } 
    else if (returns < -threshold_) {
        // Strong negative momentum -> SELL
        Signal sig;
        sig.symbol   = symbol;
        sig.side     = OrderSide::SELL;
        sig.quantity = tradeQty_;
        sig.price    = 0.0;
        std::ostringstream oss;
        oss << "Negative Momentum (" << (returns * 100.0) << "%)";
        sig.reason   = oss.str();
        cooldownCounter_[symbol] = cooldown_;
        return SignalResult(sig);
    }

    return SignalResult();
}
