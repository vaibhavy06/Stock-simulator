# Stock Market Simulator

A production-grade stock market simulator written in **C++17** featuring
realistic price simulation, a price-time-priority order matching engine,
pluggable trading strategies, portfolio management, backtesting, and
multi-threaded simulation across multiple stocks.

---

## Architecture

```
stock_simulator/
├── main.cpp                    ← CLI entry point, wires all components
├── Makefile
├── config.ini                  ← All tuneable parameters
│
├── engine/
│   ├── MarketEngine.{h,cpp}    ← GBM price simulation, multi-stock, threading
│   ├── MatchingEngine.{h,cpp}  ← Price-time priority order book & matching
│   ├── StrategyEngine.{h,cpp}  ← Pipes ticks → strategies → orders → portfolio
│   └── BacktestEngine.{h,cpp}  ← CSV replay, metrics: PnL, Sharpe, drawdown
│
├── models/
│   ├── Stock.h                 ← Price history, SMA helper, volatility
│   ├── Order.h                 ← Order types (MARKET/LIMIT), BUY/SELL, status
│   ├── Trade.h                 ← Execution record (price, qty, buyer/seller IDs)
│   └── Portfolio.h             ← Cash, positions, PnL, Sharpe ratio, drawdown
│
├── strategies/
│   ├── Strategy.h              ← Abstract base class (pure virtual onPrice/reset)
│   ├── MovingAverageStrategy.{h,cpp}  ← Dual MA crossover (golden/death cross)
│   └── MomentumStrategy.{h,cpp}       ← Rate-of-change threshold with cooldown
│
├── utils/
│   ├── Logger.{h,cpp}          ← Thread-safe singleton logger (console + file)
│   ├── CSVReader.{h,cpp}       ← CSV loader for historical price data
│   └── Config.{h,cpp}          ← INI-style config file parser
│
└── data/
    └── sample_stock_data.csv   ← 200-step GBM price paths for 4 stocks
```

---

## Build & Run

### Prerequisites
- GCC 8+ or Clang 7+ with C++17 support
- POSIX threading (`-pthread`)
- No third-party libraries required

### Quick Start

```bash
# Clone / unzip the project, then:
cd stock_simulator

# Build (release)
make

# Run interactive CLI
./stock_simulator

# Or use make shortcut
make run
```

### Manual Compilation

```bash
g++ -std=c++17 -pthread -Wall -O2 \
    main.cpp \
    engine/MarketEngine.cpp \
    engine/MatchingEngine.cpp \
    engine/StrategyEngine.cpp \
    engine/BacktestEngine.cpp \
    strategies/MovingAverageStrategy.cpp \
    strategies/MomentumStrategy.cpp \
    utils/Logger.cpp \
    utils/CSVReader.cpp \
    -o stock_simulator
```

### Debug Build (with AddressSanitizer)

```bash
make debug
./stock_simulator_debug
```

---

## CLI Menu

```
╔══════════════════════════════════════════════════════════════╗
║          STOCK MARKET SIMULATOR  v1.0                        ║
║          C++17 | Random Walk | Order Matching | Backtest     ║
╚══════════════════════════════════════════════════════════════╝

┌─────────────────────────────────────────┐
│              MAIN MENU                  │
├─────────────────────────────────────────┤
│  1. Live Simulation (Moving Average)    │
│  2. Live Simulation (Momentum)          │
│  3. Backtest: Moving Average Strategy   │
│  4. Backtest: Momentum Strategy         │
│  5. Compare Both Backtest Strategies    │
│  6. Exit                                │
└─────────────────────────────────────────┘
```

---

## Configuration (`config.ini`)

| Key | Default | Description |
|-----|---------|-------------|
| `SIMULATION_STEPS` | 100 | Number of time steps in live sim |
| `PARALLEL_SIMULATION` | true | Use std::thread per stock |
| `INITIAL_CASH` | 100000.0 | Starting portfolio cash |
| `RANDOM_SEED` | 42 | RNG seed for reproducibility |
| `LOG_FILE` | simulator.log | Output log path |
| `LOG_LEVEL` | INFO | DEBUG / INFO / WARN / ERROR |
| `MA_SHORT_PERIOD` | 5 | Moving average fast window |
| `MA_LONG_PERIOD` | 20 | Moving average slow window |
| `MA_TRADE_QUANTITY` | 10 | Shares per MA signal |
| `MOMENTUM_LOOKBACK` | 10 | Momentum look-back window |
| `MOMENTUM_THRESHOLD` | 0.02 | ±2% rate-of-change trigger |
| `MOMENTUM_COOLDOWN` | 5 | Steps to wait after a trade |
| `MOMENTUM_TRADE_QUANTITY` | 10 | Shares per momentum signal |
| `BACKTEST_CSV` | data/sample_stock_data.csv | Historical data path |
| `BACKTEST_STARTING_CASH` | 100000.0 | Backtest portfolio cash |
| `STOCK_N` | AAPL,150.0,0.015 | Symbol, start price, volatility |

---

## Core Subsystems

### 1. Market Engine — Geometric Brownian Motion
Each stock price evolves each step via:

```
S(t+1) = S(t) × exp(σ × ε)    where ε ~ N(0,1)
```

- `σ` (volatility) is per-stock and configurable
- Hard floor at $0.01 prevents negative prices
- Multi-stock parallel mode: one `std::thread` per stock, `std::mutex` protects shared RNG

### 2. Matching Engine — Price-Time Priority
- **Bid book**: `std::map<double, std::deque<Order>, std::greater<>>` — highest bid first
- **Ask book**: `std::map<double, std::deque<Order>>` — lowest ask first
- Incoming orders sweep the contra-side book at best available price
- Partial fills supported: orders split across multiple resting levels
- Market maker synthetic orders seed initial liquidity at ±0.1% spread

### 3. Strategy Engine
Abstract `Strategy` interface with two methods:
```cpp
virtual std::optional<Signal> onPrice(const std::string& symbol,
                                      double price, int64_t timestamp) = 0;
virtual void reset() = 0;
```

#### MovingAverageStrategy
- Computes SMA(5) and SMA(20) from rolling price window
- **BUY** signal: SMA(5) crosses above SMA(20) — *golden cross*
- **SELL** signal: SMA(5) crosses below SMA(20) — *death cross*
- No double-signals: tracks previous crossover state per symbol

#### MomentumStrategy
- Computes rate-of-change over `lookback` window: `(P_now - P_old) / P_old`
- **BUY** when RoC > +`threshold`; **SELL** when RoC < −`threshold`
- Cooldown timer prevents over-trading after each signal
- Tracks open/closed position state per symbol

### 4. Portfolio Manager
- Tracks cash and per-symbol `Position { quantity, avgCost, realizedPnL }`
- Average cost uses **volume-weighted cost basis** for accurate PnL
- Metrics computed on-the-fly:
  - **Realized PnL**: locked in from closed positions
  - **Unrealized PnL**: mark-to-market against current prices
  - **Max Drawdown**: rolling peak-to-trough from value history
  - **Sharpe Ratio**: annualised `mean(returns) / std(returns) × √252`
  - **Win Rate**: winning sell trades / total sell trades

### 5. Backtest Engine
- Loads CSV via `CSVReader::load()` → sorted `vector<PriceRecord>`
- Replays ticks chronologically, feeding each price to the strategy
- Executes signals as immediate fills at the tick price (no slippage model)
- Liquidates remaining open positions at last available price for fair PnL
- Reports: Total PnL, Return %, Win Rate, Max Drawdown, Sharpe Ratio

### 6. Logging System
Thread-safe singleton logger (`utils/Logger.h`):
- Writes to both `stdout` and `simulator.log` simultaneously
- Mutex-protected for multi-threaded correctness
- Log levels: DEBUG / INFO / WARN / ERROR (filter via `config.ini`)
- Flushes after every write to prevent data loss on crash

---

## Sample Output

### Backtest Results
```
╔══════════════════════════════════════════════╗
║         BACKTEST RESULTS SUMMARY             ║
╠══════════════════════════════════════════════╣
║  Strategy                     MovingAverage  ║
║  Starting Cash                      $100000  ║
║  Final Value                        $105471  ║
║  Total P&L                        $+5471.80  ║
║  Return                              +5.47%  ║
║  Total Trades                            20  ║
║  Winning Trades                           9  ║
║  Losing Trades                           11  ║
║  Win Rate                             45.0%  ║
║  Max Drawdown                       -84.19%  ║
║  Sharpe Ratio                         3.444  ║
╚══════════════════════════════════════════════╝
```

### Live Trade Feed
```
[INFO ] [STRATEGY:MovingAverage] BUY GOOGL x10 @ 3078.77 | Golden cross SMA5(3003.69) > SMA20(2984.59)
[INFO ] [TRADE] BUY GOOGL x10 @ $2993.93 | Cash: $70060.70
[INFO ] [STRATEGY:MovingAverage] SELL GOOGL x10 @ 2862.61 | Death cross SMA5(2958.88) < SMA20(2988.20)
[INFO ] [TRADE] SELL GOOGL x10 @ $2863.72 | Cash: $98697.87
```

---

## Design Principles Applied

| Principle | Implementation |
|-----------|---------------|
| **SRP** | Each class has one responsibility (matching ≠ pricing ≠ portfolio) |
| **OCP** | Add new strategies by extending `Strategy` base class only |
| **LSP** | `MovingAverageStrategy` and `MomentumStrategy` are interchangeable |
| **DIP** | `StrategyEngine` depends on `Strategy` interface, not concrete classes |
| **Encapsulation** | Order books, position data hidden behind public APIs |
| **Move semantics** | `Order` objects moved into books; `Trade` returned by value |
| **Thread safety** | All shared state protected by `std::mutex` |
| **RAII** | `ofstream` in Logger closed in destructor automatically |

---

## Adding a New Strategy

1. Create `strategies/MyStrategy.h` extending `Strategy`:
```cpp
class MyStrategy : public Strategy {
public:
    MyStrategy() : Strategy("MyStrategy") {}
    std::optional<Signal> onPrice(const std::string& sym,
                                  double price, int64_t ts) override;
    void reset() override;
};
```

2. Implement `onPrice()` — return `std::nullopt` to pass, or a `Signal` struct
3. Add to `main.cpp` menu and wire via `strategyEngine.addStrategy(...)`

---

## Extending the CSV Format

The CSV loader expects `timestamp,symbol,price` but can be extended.
Add columns to `PriceRecord` in `utils/CSVReader.h` and parse them
in `CSVReader::load()`. The backtest engine receives the full record.

---

## Known Limitations & Future Work

- **No slippage model**: backtest fills at exact tick price
- **No transaction costs**: commissions/spreads not deducted
- **Single-strategy at a time**: live sim runs one strategy (easily extended)
- **No persistence**: portfolio state lost between runs (add SQLite or JSON export)
- **Fixed lot sizes**: strategies trade fixed quantity; add position sizing
