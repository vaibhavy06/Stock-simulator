#pragma once
#include "Strategy.h"
#include <deque>
#include <unordered_map>

class MovingAverageStrategy : public Strategy {
public:
    MovingAverageStrategy(int shortPeriod = 5, int longPeriod = 20,
                          int tradeQty = 10)
        : Strategy("MovingAverage"),
          shortPeriod_(shortPeriod), longPeriod_(longPeriod),
          tradeQty_(tradeQty) {}

    SignalResult onPrice(const std::string& symbol, double price, int64_t timestamp) override;

    void reset() override { windows_.clear(); lastCross_.clear(); }

private:
    int shortPeriod_;
    int longPeriod_;
    int tradeQty_;

    std::unordered_map<std::string, std::deque<double>> windows_;
    std::unordered_map<std::string, bool> lastCross_;
};
