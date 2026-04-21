#pragma once
#include "Strategy.h"
#include <deque>
#include <unordered_map>

class MomentumStrategy : public Strategy {
public:
    MomentumStrategy(int lookback, double threshold, int cooldown, int tradeQty)
        : Strategy("Momentum"),
          lookback_(lookback), threshold_(threshold),
          cooldown_(cooldown), tradeQty_(tradeQty) {}

    SignalResult onPrice(const std::string& symbol, double price, int64_t timestamp) override;

    void reset() override { priceHistory_.clear(); cooldownCounter_.clear(); }

private:
    int    lookback_;
    double threshold_;
    int    cooldown_;
    int    tradeQty_;

    std::unordered_map<std::string, std::deque<double>> priceHistory_;
    std::unordered_map<std::string, int> cooldownCounter_;
};
