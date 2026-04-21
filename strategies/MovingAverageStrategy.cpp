#include "MovingAverageStrategy.h"
#include <sstream>

std::optional<Signal> MovingAverageStrategy::onPrice(
        const std::string& symbol, double price, int64_t /*timestamp*/) {

    auto& win = windows_[symbol];
    win.prices.push_back(price);

    // Need at least longPeriod_ data points
    if ((int)win.prices.size() > longPeriod_ + 1)
        win.prices.pop_front();

    if ((int)win.prices.size() < longPeriod_)
        return std::nullopt; // Not enough data yet

    double shortMA = sma(win.prices, shortPeriod_);
    double longMA  = sma(win.prices, longPeriod_);
    bool currentAbove = (shortMA > longMA);

    if (!win.initialized) {
        win.lastAbove   = currentAbove;
        win.initialized = true;
        return std::nullopt;
    }

    std::optional<Signal> signal;

    if (!win.lastAbove && currentAbove) {
        // Golden cross: short MA just crossed above long MA => BUY
        std::ostringstream reason;
        reason << "Golden cross SMA" << shortPeriod_ << "(" << shortMA
               << ") > SMA" << longPeriod_ << "(" << longMA << ")";
        signal = Signal{symbol, OrderSide::BUY, tradeQty_, 0.0, reason.str()};

    } else if (win.lastAbove && !currentAbove) {
        // Death cross: short MA just crossed below long MA => SELL
        std::ostringstream reason;
        reason << "Death cross SMA" << shortPeriod_ << "(" << shortMA
               << ") < SMA" << longPeriod_ << "(" << longMA << ")";
        signal = Signal{symbol, OrderSide::SELL, tradeQty_, 0.0, reason.str()};
    }

    win.lastAbove = currentAbove;
    return signal;
}
