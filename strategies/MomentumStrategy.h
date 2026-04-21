#pragma once
#include "Strategy.h"
#include <deque>
#include <unordered_map>
#include <cmath>

// Momentum Strategy
// Measures rate of price change over a lookback window.
// BUY  when momentum exceeds +threshold (strong upward momentum)
// SELL when momentum drops below -threshold (strong downward momentum)
// Also implements a cooldown to avoid over-trading.
class MomentumStrategy : public Strategy {
public:
    MomentumStrategy(int lookback = 10, double threshold = 0.02,
                     int cooldown = 5, int tradeQty = 10)
        : Strategy("Momentum"),
          lookback_(lookback), threshold_(threshold),
          cooldown_(cooldown), tradeQty_(tradeQty) {}

    std::optional<Signal> onPrice(const std::string& symbol,
                                  double price,
                                  int64_t timestamp) override;

    void reset() override {
        windows_.clear();
        cooldownCounter_.clear();
        inPosition_.clear();
    }

private:
    int    lookback_;
    double threshold_;
    int    cooldown_;
    int    tradeQty_;

    struct Window {
        std::deque<double> prices;
    };

    std::unordered_map<std::string, Window> windows_;
    std::unordered_map<std::string, int>    cooldownCounter_;
    std::unordered_map<std::string, bool>   inPosition_; // basic position tracking
};
