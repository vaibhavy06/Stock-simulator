#pragma once
#include <string>
#include <cstdint>

// Represents a completed transaction between two orders
struct Trade {
    uint64_t    id;
    std::string symbol;
    double      price;
    int         quantity;
    uint64_t    buyOrderId;
    uint64_t    sellOrderId;
    int64_t     timestamp;

    Trade() = default;

    Trade(const std::string& sym, double px, int qty,
          uint64_t buyId, uint64_t sellId, int64_t ts)
        : symbol(sym), price(px), quantity(qty),
          buyOrderId(buyId), sellOrderId(sellId), timestamp(ts) {
        static std::atomic<uint64_t> counter{1};
        id = counter.fetch_add(1, std::memory_order_relaxed);
    }

    double value() const { return price * static_cast<double>(quantity); }
};

#include <atomic>  // needed for std::atomic in Trade ctor
