#include "StrategyEngine.h"
#include <sstream>

void StrategyEngine::onPrice(const std::string& symbol,
                              double price, int64_t timestamp) {
    for (auto& strategy : strategies_) {
        auto signalRes = strategy->onPrice(symbol, price, timestamp);
        if (!signalRes.hasValue) continue;
        
        auto& signal = signalRes.value;

        // Validate signal against portfolio state
        if (signal.side == OrderSide::BUY) {
            double cost = price * signal.quantity;
            if (!canBuy(symbol, signal.quantity, price)) {
                std::ostringstream oss;
                oss << "[" << strategy->name() << "] SKIP BUY " << symbol
                    << " x" << signal.quantity << " - insufficient cash ($"
                    << portfolio_.cash() << " needed ~$" << cost << ")";
                LOG_DEBUG(oss.str());
                continue;
            }
        } else {
            if (!canSell(symbol, signal.quantity)) {
                std::ostringstream oss;
                oss << "[" << strategy->name() << "] SKIP SELL " << symbol
                    << " x" << signal.quantity << " - no position";
                LOG_DEBUG(oss.str());
                continue;
            }
        }

        // Build and submit order
        OrderType otype = signal.isMarket() ? OrderType::MARKET : OrderType::LIMIT;
        Order order(symbol, otype, signal.side,
                    signal.isMarket() ? price : signal.price,
                    signal.quantity, timestamp, strategy->name());

        std::ostringstream log;
        log << "[STRATEGY:" << strategy->name() << "] "
            << order.sideStr() << " " << symbol
            << " x" << signal.quantity
            << " @ " << (signal.isMarket() ? price : signal.price)
            << " | " << signal.reason;
        LOG_INFO(log.str());

        {
            std::lock_guard<std::mutex> lock(mutex_);
            pendingOrders_[order.id] = {signal.side, symbol};
        }
        matchingEngine_.submitOrder(order, timestamp);
    }
}

void StrategyEngine::onTrade(const Trade& trade) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Determine if this was a buy or sell by checking pending orders
    OrderSide side = OrderSide::BUY;
    auto it = pendingOrders_.find(trade.buyOrderId);
    if (it != pendingOrders_.end()) {
        side = OrderSide::BUY;
        pendingOrders_.erase(it);
    } else {
        auto it2 = pendingOrders_.find(trade.sellOrderId);
        if (it2 != pendingOrders_.end()) {
            side = OrderSide::SELL;
            pendingOrders_.erase(it2);
        } else {
            return; // Trade from market maker, not our strategy
        }
    }

    try {
        portfolio_.applyTrade(trade, side);

        std::ostringstream log;
        log << "[TRADE] " << (side == OrderSide::BUY ? "BUY" : "SELL")
            << " " << trade.symbol
            << " x" << trade.quantity
            << " @ $" << trade.price
            << " | Cash: $" << portfolio_.cash();
        LOG_INFO(log.str());

        // Record win/loss for sell trades
        if (side == OrderSide::SELL) {
            const auto* pos = portfolio_.position(trade.symbol);
            if (pos) {
                portfolio_.recordTradeResult(pos->realizedPnL > 0);
            }
        }

    } catch (const std::exception& e) {
        LOG_WARN(std::string("[TRADE ERROR] ") + e.what());
    }
}

bool StrategyEngine::canBuy(const std::string& /*sym*/,
                              int qty, double price) const {
    return portfolio_.cash() >= price * qty;
}

bool StrategyEngine::canSell(const std::string& symbol, int qty) const {
    const auto* pos = portfolio_.position(symbol);
    return pos && pos->quantity >= qty;
}
