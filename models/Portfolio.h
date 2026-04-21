#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include "Trade.h"

struct Position {
    std::string symbol;
    int   quantity   = 0;
    double avgCost   = 0.0;   // Volume-weighted average cost
    double realizedPnL = 0.0; // Locked in profit from closed positions

    double unrealizedPnL(double currentPrice) const {
        return (currentPrice - avgCost) * static_cast<double>(quantity);
    }

    double marketValue(double currentPrice) const {
        return currentPrice * static_cast<double>(quantity);
    }
};

class Portfolio {
public:
    explicit Portfolio(double initialCash)
        : cash_(initialCash), initialCash_(initialCash) {}

    // Apply a completed trade to positions and cash
    void applyTrade(const Trade& trade, OrderSide side) {
        auto& pos = positions_[trade.symbol];
        pos.symbol = trade.symbol;

        if (side == OrderSide::BUY) {
            double totalCost = trade.price * trade.quantity;
            if (totalCost > cash_)
                throw std::runtime_error("Insufficient cash for trade");
            cash_ -= totalCost;
            // Update average cost
            double prevTotal = pos.avgCost * pos.quantity;
            pos.quantity += trade.quantity;
            pos.avgCost = (prevTotal + totalCost) / pos.quantity;

        } else { // SELL
            if (pos.quantity < trade.quantity)
                throw std::runtime_error("Insufficient position to sell");
            double proceeds = trade.price * trade.quantity;
            cash_ += proceeds;
            pos.realizedPnL += (trade.price - pos.avgCost) * trade.quantity;
            pos.quantity -= trade.quantity;
            if (pos.quantity == 0) pos.avgCost = 0.0;
        }

        tradeHistory_.push_back(trade);
        updatePeakValue(cash_); // will add market value tracking separately
    }

    double cash() const { return cash_; }

    const Position* position(const std::string& symbol) const {
        auto it = positions_.find(symbol);
        return it != positions_.end() ? &it->second : nullptr;
    }

    // Total realized P&L across all positions
    double totalRealizedPnL() const {
        double total = 0.0;
        for (auto const& it : positions_) total += it.second.realizedPnL;
        return total;
    }

    // Total unrealized P&L given current prices map
    double totalUnrealizedPnL(const std::unordered_map<std::string, double>& prices) const {
        double total = 0.0;
        for (auto const& it : positions_) {
            const std::string& sym = it.first;
            const auto& pos = it.second;
            if (pos.quantity > 0) {
                auto itPrice = prices.find(sym);
                if (itPrice != prices.end())
                    total += pos.unrealizedPnL(itPrice->second);
            }
        }
        return total;
    }

    // Total portfolio value = cash + market value of all positions
    double totalValue(const std::unordered_map<std::string, double>& prices) const {
        double total = cash_;
        for (auto const& it : positions_) {
            const std::string& sym = it.first;
            const auto& pos = it.second;
            if (pos.quantity > 0) {
                auto itPrice = prices.find(sym);
                if (itPrice != prices.end())
                    total += pos.marketValue(itPrice->second);
            }
        }
        return total;
    }

    // Record portfolio value for drawdown tracking
    void recordValue(double val) {
        valueHistory_.push_back(val);
        peakValue_ = std::max(peakValue_, val);
    }

    // Maximum drawdown from peak (returns negative or zero percentage)
    double maxDrawdown() const {
        if (valueHistory_.empty()) return 0.0;
        double peak = valueHistory_[0];
        double maxDD = 0.0;
        for (double v : valueHistory_) {
            peak = std::max(peak, v);
            double dd = (v - peak) / peak;
            maxDD = std::min(maxDD, dd);
        }
        return maxDD * 100.0; // as percentage
    }

    // Sharpe Ratio: (mean return - risk_free) / std_dev of returns
    // Uses daily returns from value history; risk_free = 0 for simplicity
    double sharpeRatio() const {
        if (valueHistory_.size() < 2) return 0.0;
        std::vector<double> returns;
        for (size_t i = 1; i < valueHistory_.size(); ++i) {
            if (valueHistory_[i - 1] != 0.0)
                returns.push_back((valueHistory_[i] - valueHistory_[i-1]) / valueHistory_[i-1]);
        }
        if (returns.empty()) return 0.0;
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double var  = 0.0;
        for (double r : returns) var += (r - mean) * (r - mean);
        var /= returns.size();
        double stddev = std::sqrt(var);
        if (stddev < 1e-10) return 0.0;
        return mean / stddev * std::sqrt(252.0); // annualized
    }

    // Win rate among completed trades (sell-side)
    double winRate() const {
        if (tradeResults_.empty()) return 0.0;
        int wins = 0;
        for (bool w : tradeResults_) if (w) wins++;
        return static_cast<double>(wins) / tradeResults_.size() * 100.0;
    }

    void recordTradeResult(bool isWin) { tradeResults_.push_back(isWin); }

    const std::vector<Trade>& tradeHistory() const { return tradeHistory_; }
    const std::unordered_map<std::string, Position>& positions() const { return positions_; }
    double initialCash() const { return initialCash_; }

private:
    double cash_;
    double initialCash_;
    double peakValue_ = 0.0;
    std::unordered_map<std::string, Position> positions_;
    std::vector<Trade> tradeHistory_;
    std::vector<double> valueHistory_;
    std::vector<bool> tradeResults_;

    void updatePeakValue(double v) { peakValue_ = std::max(peakValue_, v); }
};

// Bring in OrderSide without circular include
#include "../models/Order.h"
