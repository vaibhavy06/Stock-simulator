#pragma once
#include "Strategy.h"
#include <deque>
#include <unordered_map>
#include <numeric>

// Dual Moving Average Crossover Strategy
// BUY  when short MA crosses ABOVE long MA (golden cross)
// SELL when short MA crosses BELOW long MA (death cross)
class MovingAverageStrategy : public Strategy {
public:
    MovingAverageStrategy(int shortPeriod = 5, int longPeriod = 20,
                          int tradeQty = 10)
        : Strategy("MovingAverage"),
          shortPeriod_(shortPeriod), longPeriod_(longPeriod),
          tradeQty_(tradeQty) {}

    std::optional<Signal> onPrice(const std::string& symbol,
                                  double price,
                                  int64_t timestamp) override;

    void reset() override { windows_.clear(); lastCross_.clear(); }

private:
    int shortPeriod_;
    int longPeriod_;
    int tradeQty_;

    struct Window {
        std::deque<double> prices;
        bool lastAbove = false;  // was short MA above long MA last step?
        bool initialized = false;
    };

    std::unordered_map<std::string, Window> windows_;
    std::unordered_map<std::string, bool>   lastCross_; // prevent double signals

    double sma(const std::deque<double>& data, int periods) const {
        double sum = 0.0;
        auto it = data.end();
        for (int i = 0; i < periods; ++i) sum += *(--it);
        return sum / periods;
    }
};
