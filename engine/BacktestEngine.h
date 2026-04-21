#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../strategies/Strategy.h"
#include "../models/Portfolio.h"
#include "../utils/CSVReader.h"

struct BacktestResult {
    std::string strategyName;
    double      startingCash;
    double      finalValue;
    double      totalPnL;
    double      returnPct;
    double      winRate;
    double      maxDrawdown;
    double      sharpeRatio;
    int         totalTrades;
    int         winningTrades;
    int         losingTrades;
};

class BacktestEngine {
public:
    BacktestEngine(double startingCash, const std::string& csvPath);

    // Run backtest with a given strategy; returns result metrics
    BacktestResult run(std::shared_ptr<Strategy> strategy);

    // Print a formatted result summary
    static void printResult(const BacktestResult& result);

private:
    double startingCash_;
    std::vector<PriceRecord> data_;
    std::unordered_map<std::string, std::vector<PriceRecord>> bySymbol_;

    // Simulate one step: feed price to strategy, execute signals on portfolio
    void processSignal(const Signal& signal, double price,
                       int64_t timestamp, Portfolio& portfolio,
                       std::vector<bool>& tradeResults,
                       int& wins, int& losses);
};
