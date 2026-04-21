#include "BacktestEngine.h"
#include "../utils/Logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>

BacktestEngine::BacktestEngine(double startingCash, const std::string& csvPath)
    : startingCash_(startingCash) {
    data_     = CSVReader::load(csvPath);
    bySymbol_ = CSVReader::groupBySymbol(data_);

    std::ostringstream oss;
    oss << "[BACKTEST] Loaded " << data_.size() << " records for "
        << bySymbol_.size() << " symbol(s)";
    LOG_INFO(oss.str());
}

BacktestResult BacktestEngine::run(std::shared_ptr<Strategy> strategy) {
    strategy->reset();
    Portfolio portfolio(startingCash_);
    int wins = 0, losses = 0;
    std::vector<bool> tradeResults;

    // Track per-symbol average cost for PnL computation in backtest
    std::unordered_map<std::string, double> avgCost;
    std::unordered_map<std::string, int>    holdings;

    LOG_INFO("[BACKTEST] Starting run with strategy: " + strategy->name());

    // Replay data in timestamp order
    for (const auto& rec : data_) {
        auto signalRes = strategy->onPrice(rec.symbol, rec.price, rec.timestamp);
        if (!signalRes.hasValue) {
            // Still record portfolio value for drawdown
            std::unordered_map<std::string, double> prices;
            for (auto const& it : holdings) prices[it.first] = rec.price;
            double val = portfolio.cash();
            for (auto const& it : holdings) {
                const std::string& sym = it.first;
                int qty = it.second;
                auto itPrice = prices.find(sym);
                if (itPrice != prices.end()) val += qty * itPrice->second;
            }
            portfolio.recordValue(val);
            continue;
        }
        
        auto& signal = signalRes.value;

        processSignal(signal, rec.price, rec.timestamp,
                      portfolio, tradeResults, wins, losses);

        // Update holdings tracking
        if (signal.side == OrderSide::BUY) {
            holdings[rec.symbol]  += signal.quantity;
            double prev = avgCost[rec.symbol] * (holdings[rec.symbol] - signal.quantity);
            avgCost[rec.symbol] = (prev + rec.price * signal.quantity) / holdings[rec.symbol];
        } else {
            holdings[rec.symbol] = std::max(0, holdings[rec.symbol] - signal.quantity);
            if (holdings[rec.symbol] == 0) avgCost[rec.symbol] = 0.0;
        }

        // Record portfolio value snapshot for drawdown / Sharpe
        double val = portfolio.cash();
        for (auto const& it : holdings) {
            val += it.second * rec.price;
        }
        portfolio.recordValue(val);
    }

    // Liquidate remaining positions at last known price
    std::unordered_map<std::string, double> lastPrice;
    for (auto& rec : data_) lastPrice[rec.symbol] = rec.price;

    double finalCash = portfolio.cash();
    for (auto const& it : holdings) {
        const std::string& sym = it.first;
        int qty = it.second;
        if (qty > 0 && lastPrice.count(sym)) {
            finalCash += qty * lastPrice[sym];
        }
    }

    BacktestResult result;
    result.strategyName   = strategy->name();
    result.startingCash   = startingCash_;
    result.finalValue     = finalCash;
    result.totalPnL       = finalCash - startingCash_;
    result.returnPct      = (result.totalPnL / startingCash_) * 100.0;
    result.winningTrades  = wins;
    result.losingTrades   = losses;
    result.totalTrades    = wins + losses;
    result.winRate        = result.totalTrades > 0
                            ? (double)wins / result.totalTrades * 100.0
                            : 0.0;
    result.maxDrawdown    = portfolio.maxDrawdown();
    result.sharpeRatio    = portfolio.sharpeRatio();

    return result;
}

void BacktestEngine::processSignal(const Signal& signal, double price,
                                    int64_t timestamp, Portfolio& portfolio,
                                    std::vector<bool>& /*tradeResults*/,
                                    int& wins, int& losses) {
    // Simplified direct execution for backtesting (no order book)
    Trade trade(signal.symbol, price, signal.quantity,
                0, 0, timestamp);

    try {
        if (signal.side == OrderSide::BUY) {
            if (portfolio.cash() < price * signal.quantity) return;
            portfolio.applyTrade(trade, OrderSide::BUY);

            std::ostringstream log;
            log << "[BT] BUY  " << signal.symbol
                << " x" << signal.quantity
                << " @ $" << std::fixed << std::setprecision(2) << price
                << " | Cash: $" << portfolio.cash();
            LOG_DEBUG(log.str());

        } else { // SELL
            const auto* pos = portfolio.position(signal.symbol);
            if (!pos || pos->quantity < signal.quantity) return;

            double costBasis = pos->avgCost; // capture before applyTrade modifies it
            bool isWin = price > costBasis;

            portfolio.applyTrade(trade, OrderSide::SELL);

            if (isWin) wins++; else losses++;
            portfolio.recordTradeResult(isWin);

            std::ostringstream log;
            log << "[BT] SELL " << signal.symbol
                << " x" << signal.quantity
                << " @ $" << std::fixed << std::setprecision(2) << price
                << " PnL/share: $" << (price - costBasis)
                << " | Cash: $" << portfolio.cash();
            LOG_DEBUG(log.str());
        }
    } catch (const std::exception& e) {
        LOG_WARN(std::string("[BT ERROR] ") + e.what());
    }
}

void BacktestEngine::printResult(const BacktestResult& result) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════╗\n";
    std::cout << "║         BACKTEST RESULTS SUMMARY             ║\n";
    std::cout << "╠══════════════════════════════════════════════╣\n";
    auto line = [](const std::string& label, const std::string& value) {
        std::cout << "║  " << std::left << std::setw(22) << label
                  << std::right << std::setw(20) << value << "  ║\n";
    };
    line("Strategy",       result.strategyName);
    line("Starting Cash",  "$" + std::to_string((int)result.startingCash));
    line("Final Value",    "$" + std::to_string((int)result.finalValue));

    std::ostringstream pnl;
    pnl << std::fixed << std::setprecision(2)
        << (result.totalPnL >= 0 ? "+" : "") << result.totalPnL;
    line("Total P&L",      "$" + pnl.str());

    std::ostringstream ret;
    ret << std::fixed << std::setprecision(2)
        << (result.returnPct >= 0 ? "+" : "") << result.returnPct << "%";
    line("Return",         ret.str());

    line("Total Trades",   std::to_string(result.totalTrades));
    line("Winning Trades", std::to_string(result.winningTrades));
    line("Losing Trades",  std::to_string(result.losingTrades));

    std::ostringstream wr;
    wr << std::fixed << std::setprecision(1) << result.winRate << "%";
    line("Win Rate",       wr.str());

    std::ostringstream dd;
    dd << std::fixed << std::setprecision(2) << result.maxDrawdown << "%";
    line("Max Drawdown",   dd.str());

    std::ostringstream sr;
    sr << std::fixed << std::setprecision(3) << result.sharpeRatio;
    line("Sharpe Ratio",   sr.str());

    std::cout << "╚══════════════════════════════════════════════╝\n\n";
}
