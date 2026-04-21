#include "MatchingEngine.h"
#include <stdexcept>

// ─── Public Interface ────────────────────────────────────────────────────────

void MatchingEngine::submitOrder(Order order, int64_t ts) {
    std::lock_guard<std::mutex> lock(mutex_);
    matchOrder(order, ts);
    if (order.isActive()) {
        addToBook(order);
    }
}

bool MatchingEngine::cancelOrder(uint64_t orderId,
                                  const std::string& symbol,
                                  OrderSide side) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (side == OrderSide::BUY) {
        auto& bids = bidBooks_[symbol];
        for (auto& [price, dq] : bids) {
            for (auto it = dq.begin(); it != dq.end(); ++it) {
                if (it->id == orderId) {
                    it->status = OrderStatus::CANCELLED;
                    dq.erase(it);
                    return true;
                }
            }
        }
    } else {
        auto& asks = askBooks_[symbol];
        for (auto& [price, dq] : asks) {
            for (auto it = dq.begin(); it != dq.end(); ++it) {
                if (it->id == orderId) {
                    it->status = OrderStatus::CANCELLED;
                    dq.erase(it);
                    return true;
                }
            }
        }
    }
    return false;
}

const BidBook& MatchingEngine::bidBook(const std::string& symbol) const {
    static BidBook empty;
    auto it = bidBooks_.find(symbol);
    return it != bidBooks_.end() ? it->second : empty;
}

const AskBook& MatchingEngine::askBook(const std::string& symbol) const {
    static AskBook empty;
    auto it = askBooks_.find(symbol);
    return it != askBooks_.end() ? it->second : empty;
}

double MatchingEngine::bestBid(const std::string& symbol) const {
    auto it = bidBooks_.find(symbol);
    if (it == bidBooks_.end() || it->second.empty()) return 0.0;
    return it->second.begin()->first;
}

double MatchingEngine::bestAsk(const std::string& symbol) const {
    auto it = askBooks_.find(symbol);
    if (it == askBooks_.end() || it->second.empty()) return 0.0;
    return it->second.begin()->first;
}

// ─── Private Matching Logic ──────────────────────────────────────────────────

void MatchingEngine::matchOrder(Order& incoming, int64_t ts) {
    const std::string& sym = incoming.symbol;

    if (incoming.side == OrderSide::BUY) {
        // BUY order: match against ask book (lowest asks first)
        auto& asks = askBooks_[sym];
        auto it = asks.begin();

        while (it != asks.end() && incoming.remainingQty() > 0) {
            double askPrice = it->first;

            // For MARKET orders, always match; for LIMIT, only if ask <= limit
            bool priceOk = (incoming.type == OrderType::MARKET) ||
                           (incoming.price >= askPrice);
            if (!priceOk) break;

            auto& dq = it->second;
            while (!dq.empty() && incoming.remainingQty() > 0) {
                Order& resting = dq.front();
                int matched = std::min(incoming.remainingQty(), resting.remainingQty());
                double execPrice = askPrice; // price priority: resting order's price

                Trade t = makeTrade(sym, execPrice, matched,
                                    incoming.id, resting.id, ts);
                trades_.push_back(t);
                if (tradeCallback_) tradeCallback_(t);

                incoming.filledQty += matched;
                resting.filledQty  += matched;

                if (resting.isFilled() || resting.remainingQty() == 0) {
                    resting.status = OrderStatus::FILLED;
                    dq.pop_front();
                } else {
                    resting.status = OrderStatus::PARTIALLY_FILLED;
                }
            }
            if (dq.empty()) it = asks.erase(it);
            else ++it;
        }

    } else { // SELL order: match against bid book (highest bids first)
        auto& bids = bidBooks_[sym];
        auto it = bids.begin();

        while (it != bids.end() && incoming.remainingQty() > 0) {
            double bidPrice = it->first;

            bool priceOk = (incoming.type == OrderType::MARKET) ||
                           (incoming.price <= bidPrice);
            if (!priceOk) break;

            auto& dq = it->second;
            while (!dq.empty() && incoming.remainingQty() > 0) {
                Order& resting = dq.front();
                int matched = std::min(incoming.remainingQty(), resting.remainingQty());
                double execPrice = bidPrice;

                Trade t = makeTrade(sym, execPrice, matched,
                                    resting.id, incoming.id, ts);
                trades_.push_back(t);
                if (tradeCallback_) tradeCallback_(t);

                incoming.filledQty += matched;
                resting.filledQty  += matched;

                if (resting.remainingQty() == 0) {
                    resting.status = OrderStatus::FILLED;
                    dq.pop_front();
                } else {
                    resting.status = OrderStatus::PARTIALLY_FILLED;
                }
            }
            if (dq.empty()) it = bids.erase(it);
            else ++it;
        }
    }

    // Update incoming order status
    if (incoming.filledQty >= incoming.quantity) {
        incoming.status = OrderStatus::FILLED;
    } else if (incoming.filledQty > 0) {
        incoming.status = OrderStatus::PARTIALLY_FILLED;
    }
}

void MatchingEngine::addToBook(const Order& order) {
    if (order.type == OrderType::MARKET) return; // Market orders don't rest
    if (order.side == OrderSide::BUY) {
        bidBooks_[order.symbol][order.price].push_back(order);
    } else {
        askBooks_[order.symbol][order.price].push_back(order);
    }
}

Trade MatchingEngine::makeTrade(const std::string& symbol,
                                 double price, int qty,
                                 uint64_t buyId, uint64_t sellId,
                                 int64_t ts) {
    return Trade(symbol, price, qty, buyId, sellId, ts);
}
