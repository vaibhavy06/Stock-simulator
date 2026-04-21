/**
 * Stock Market Simulator — main.cpp
 *
 * Entry point. Provides a CLI menu to:
 *   1. Run live simulation with a chosen strategy
 *   2. Run backtests (Moving Average / Momentum)
 *   3. View portfolio summary
 *   4. Exit
 *
 * Architecture:
 *   MarketEngine     → drives price simulation (random walk / GBM)
 *   MatchingEngine   → maintains order books, matches buy/sell orders
 *   StrategyEngine   → pipes price ticks to strategies, places orders
 *   BacktestEngine   → replays CSV data, evaluates strategy performance
 *   Portfolio        → tracks cash, positions, PnL, Sharpe, drawdown
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include <limits>
#include <chrono>
#include <thread>

// Engine layer
#include "engine/MarketEngine.h"
#include "engine/MatchingEngine.h"
#include "engine/StrategyEngine.h"
#include "engine/BacktestEngine.h"

// Strategies
#include "strategies/MovingAverageStrategy.h"
#include "strategies/MomentumStrategy.h"

// Models
#include "models/Portfolio.h"

// Utilities
#include "utils/Logger.h"
#include "utils/Config.h"

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║          STOCK MARKET SIMULATOR  v1.0                        ║
║          C++17 | Random Walk | Order Matching | Backtest     ║
╚══════════════════════════════════════════════════════════════╝
)" << "\n";
}

static void printMainMenu() {
    std::cout << "┌─────────────────────────────────────────┐\n";
    std::cout << "│              MAIN MENU                  │\n";
    std::cout << "├─────────────────────────────────────────┤\n";
    std::cout << "│  1. Live Simulation (Moving Average)    │\n";
    std::cout << "│  2. Live Simulation (Momentum)          │\n";
    std::cout << "│  3. Backtest: Moving Average Strategy   │\n";
    std::cout << "│  4. Backtest: Momentum Strategy         │\n";
    std::cout << "│  5. Compare Both Backtest Strategies    │\n";
    std::cout << "│  6. Exit                                │\n";
    std::cout << "└─────────────────────────────────────────┘\n";
    std::cout << "  Enter choice: ";
}

static void printPortfolio(const Portfolio& portfolio,
                            const std::unordered_map<std::string, double>& prices) {
    std::cout << "\n╔══════════════════════════════════════════════════════╗\n";
    std::cout <<   "║                  PORTFOLIO SUMMARY                  ║\n";
    std::cout <<   "╠══════════════════════════════════════════════════════╣\n";
    std::cout << "║  Cash Balance   : $" << std::fixed << std::setprecision(2)
              << std::setw(15) << portfolio.cash() << "                  ║\n";

    double totalValue = portfolio.totalValue(prices);
    std::cout << "║  Total Value    : $" << std::setw(15) << totalValue
              << "                  ║\n";

    double realizedPnL = portfolio.totalRealizedPnL();
    std::cout << "║  Realized PnL   : $" << std::setw(15) << realizedPnL
              << "                  ║\n";

    double unrealizedPnL = portfolio.totalUnrealizedPnL(prices);
    std::cout << "║  Unrealized PnL : $" << std::setw(15) << unrealizedPnL
              << "                  ║\n";

    std::cout << "╠══════════════════════════════════════════════════════╣\n";
    std::cout << "║  HOLDINGS                                            ║\n";
    std::cout << "╠══════════════════════════════════════════════════════╣\n";

    bool hasPositions = false;
    for (auto& [sym, pos] : portfolio.positions()) {
        if (pos.quantity > 0) {
            hasPositions = true;
            auto it = prices.find(sym);
            double curPrice = (it != prices.end()) ? it->second : pos.avgCost;
            double upnl     = pos.unrealizedPnL(curPrice);

            std::cout << "║  " << std::left << std::setw(6) << sym
                      << "  qty:" << std::right << std::setw(5) << pos.quantity
                      << "  avg:$" << std::setw(8) << std::fixed << std::setprecision(2) << pos.avgCost
                      << "  cur:$" << std::setw(8) << curPrice
                      << "  uPnL:$" << std::setw(8) << upnl << "  ║\n";
        }
    }
    if (!hasPositions)
        std::cout << "║  (no open positions)                                 ║\n";

    std::cout << "╠══════════════════════════════════════════════════════╣\n";
    std::cout << "║  Win Rate       : " << std::setw(6) << std::fixed << std::setprecision(1)
              << portfolio.winRate() << "%                                ║\n";
    std::cout << "║  Max Drawdown   : " << std::setw(6) << portfolio.maxDrawdown()
              << "%                                ║\n";
    std::cout << "║  Sharpe Ratio   : " << std::setw(6) << std::setprecision(3)
              << portfolio.sharpeRatio()
              << "                                 ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n\n";
}

// ─── Live Simulation ─────────────────────────────────────────────────────────

static void runLiveSimulation(std::shared_ptr<Strategy> strategy,
                               const Config& cfg) {
    std::cout << "\n[SIM] Starting live simulation with strategy: "
              << strategy->name() << "\n";
    std::cout << "[SIM] Press ENTER after each step, or type 'q' to quit early.\n\n";

    double initCash    = cfg.getDouble("INITIAL_CASH", 100000.0);
    int    steps       = cfg.getInt("SIMULATION_STEPS", 100);
    bool   parallel    = cfg.getBool("PARALLEL_SIMULATION", false);

    Portfolio      portfolio(initCash);
    MatchingEngine matchingEngine;
    MarketEngine   marketEngine(matchingEngine,
                                (uint32_t)cfg.getInt("RANDOM_SEED", 42));
    StrategyEngine strategyEngine(matchingEngine, portfolio);

    strategyEngine.addStrategy(strategy);

    // Wire price updates from market engine -> strategy engine
    marketEngine.onPriceUpdate([&](const std::string& sym, double price, int64_t ts) {
        strategyEngine.onPrice(sym, price, ts);
    });

    // Add stocks from config
    for (int i = 0; i < 10; ++i) {
        std::string key = "STOCK_" + std::to_string(i);
        std::string val = cfg.getString(key);
        if (val.empty()) break;

        std::istringstream ss(val);
        std::string sym, priceStr, volStr;
        std::getline(ss, sym,     ',');
        std::getline(ss, priceStr,',');
        std::getline(ss, volStr,  ',');

        if (!sym.empty() && !priceStr.empty() && !volStr.empty()) {
            marketEngine.addStock(sym, std::stod(priceStr), std::stod(volStr));
            std::cout << "[SIM] Registered stock: " << sym
                      << " @ $" << priceStr << "\n";
        }
    }

    std::cout << "\n";

    for (int step = 1; step <= steps; ++step) {
        // Check for quit
        // (non-blocking: just step through automatically with a small sleep)

        if (parallel)
            marketEngine.stepAllParallel(step);
        else
            marketEngine.stepAll(step);

        // Print price bar every 10 steps
        if (step % 10 == 0) {
            std::cout << "\n[Step " << std::setw(4) << step << "] Prices: ";
            for (auto& [sym, stk] : marketEngine.stocks()) {
                std::cout << sym << " $" << std::fixed << std::setprecision(2)
                          << stk.currentPrice << "  ";
            }
            std::cout << "\n";

            auto prices = marketEngine.currentPrices();
            double val  = portfolio.totalValue(prices);
            double pnl  = val - initCash;
            std::cout << "           Portfolio: $" << std::fixed
                      << std::setprecision(2) << val
                      << "  PnL: " << (pnl >= 0 ? "+" : "") << pnl << "\n";
            portfolio.recordValue(val);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // Final summary
    auto prices = marketEngine.currentPrices();
    printPortfolio(portfolio, prices);
}

// ─── Backtest Runner ─────────────────────────────────────────────────────────

static void runBacktest(std::shared_ptr<Strategy> strategy, const Config& cfg) {
    std::string csvPath = cfg.getString("BACKTEST_CSV", "data/sample_stock_data.csv");
    double startCash    = cfg.getDouble("BACKTEST_STARTING_CASH", 100000.0);

    std::cout << "\n[BACKTEST] Running " << strategy->name()
              << " on: " << csvPath << "\n";
    std::cout << "[BACKTEST] Starting cash: $" << startCash << "\n\n";

    try {
        BacktestEngine engine(startCash, csvPath);
        BacktestResult result = engine.run(strategy);
        BacktestEngine::printResult(result);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Backtest failed: " << e.what() << "\n";
    }
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char* /*argv*/[]) {
    // Load config
    Config& cfg = Config::instance();
    cfg.load("config.ini");

    // Initialise logger
    std::string lvlStr = cfg.getString("LOG_LEVEL", "INFO");
    LogLevel lvl = LogLevel::INFO;
    if      (lvlStr == "DEBUG") lvl = LogLevel::DEBUG;
    else if (lvlStr == "WARN")  lvl = LogLevel::WARN;
    else if (lvlStr == "ERROR") lvl = LogLevel::ERROR;
    Logger::instance().init(cfg.getString("LOG_FILE", "simulator.log"), lvl);

    printBanner();
    LOG_INFO("Simulator started");

    bool running = true;
    while (running) {
        printMainMenu();

        int choice = 0;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        // Build strategies from config
        auto buildMA = [&]() -> std::shared_ptr<Strategy> {
            return std::make_shared<MovingAverageStrategy>(
                cfg.getInt("MA_SHORT_PERIOD", 5),
                cfg.getInt("MA_LONG_PERIOD",  20),
                cfg.getInt("MA_TRADE_QUANTITY", 10));
        };
        auto buildMom = [&]() -> std::shared_ptr<Strategy> {
            return std::make_shared<MomentumStrategy>(
                cfg.getInt("MOMENTUM_LOOKBACK",        10),
                cfg.getDouble("MOMENTUM_THRESHOLD",    0.02),
                cfg.getInt("MOMENTUM_COOLDOWN",        5),
                cfg.getInt("MOMENTUM_TRADE_QUANTITY",  10));
        };

        switch (choice) {
            case 1:
                runLiveSimulation(buildMA(), cfg);
                break;

            case 2:
                runLiveSimulation(buildMom(), cfg);
                break;

            case 3:
                runBacktest(buildMA(), cfg);
                break;

            case 4:
                runBacktest(buildMom(), cfg);
                break;

            case 5: {
                std::cout << "\n══ Comparing strategies on same dataset ══\n";
                runBacktest(buildMA(),  cfg);
                runBacktest(buildMom(), cfg);
                break;
            }

            case 6:
                running = false;
                std::cout << "\nGoodbye. Check simulator.log for full trade history.\n\n";
                break;

            default:
                std::cout << "Invalid choice. Please enter 1-6.\n";
        }
    }

    LOG_INFO("Simulator exited normally");
    return 0;
}
