#include "MomentumStrategy.h"
#include <sstream>

std::optional<Signal> MomentumStrategy::onPrice(
        const std::string& symbol, double price, int64_t /*timestamp*/) {

    auto& win = windows_[symbol];
    win.prices.push_back(price);
    if ((int)win.prices.size() > lookback_ + 1) win.prices.pop_front();

    // Manage cooldown timer
    auto& cd = cooldownCounter_[symbol];
    if (cd > 0) { --cd; return std::nullopt; }

    if ((int)win.prices.size() < lookback_) return std::nullopt;

    // Rate of change: (current - oldest) / oldest
    double oldest   = win.prices.front();
    double momentum = (price - oldest) / oldest; // fractional change

    auto& inPos = inPosition_[symbol];
    std::optional<Signal> signal;

    if (!inPos && momentum > threshold_) {
        std::ostringstream reason;
        reason << "Positive momentum " << (momentum * 100.0) << "% > "
               << (threshold_ * 100.0) << "% threshold";
        signal = Signal{symbol, OrderSide::BUY, tradeQty_, 0.0, reason.str()};
        inPos = true;
        cd = cooldown_;

    } else if (inPos && momentum < -threshold_) {
        std::ostringstream reason;
        reason << "Negative momentum " << (momentum * 100.0) << "% < -"
               << (threshold_ * 100.0) << "% threshold";
        signal = Signal{symbol, OrderSide::SELL, tradeQty_, 0.0, reason.str()};
        inPos = false;
        cd = cooldown_;
    }

    return signal;
}
