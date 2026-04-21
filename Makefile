# ─── Stock Market Simulator — Makefile ───────────────────────────────────────
# Targets:
#   make          → build release binary (./stock_simulator)
#   make debug    → build with debug symbols and sanitizers
#   make clean    → remove build artefacts
#   make run      → build + run
#   make test     → run headless backtest comparison and exit

CXX      := g++
CXXFLAGS := -std=c++17 -pthread -Wall -Wextra -O2
DBGFLAGS := -std=c++17 -pthread -Wall -Wextra -g -fsanitize=address,undefined
TARGET   := stock_simulator

SRCS := main.cpp \
        engine/MarketEngine.cpp \
        engine/MatchingEngine.cpp \
        engine/StrategyEngine.cpp \
        engine/BacktestEngine.cpp \
        strategies/MovingAverageStrategy.cpp \
        strategies/MomentumStrategy.cpp \
        utils/Logger.cpp \
        utils/CSVReader.cpp

.PHONY: all debug clean run test

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "Build successful → ./$(TARGET)"

debug: $(SRCS)
	$(CXX) $(DBGFLAGS) $^ -o $(TARGET)_debug
	@echo "Debug build → ./$(TARGET)_debug"

clean:
	rm -f $(TARGET) $(TARGET)_debug simulator.log

run: all
	./$(TARGET)

# Non-interactive test: run both backtests then exit
test: all
	@echo "5\n6" | ./$(TARGET)
