#pragma once
#include <string>
#include <cstdint>
#include <chrono>
#include <atomic>

enum class OrderType  { MARKET, LIMIT };
enum class OrderSide  { BUY, SELL };
enum class OrderStatus { PENDING, PARTIALLY_FILLED, FILLED, CANCELLED };

struct Order {
    uint64_t    id;
    std::string symbol;
    OrderType   type;
    OrderSide   side;
    double      price;       // Ignored for MARKET orders
    int         quantity;
    int         filledQty;
    OrderStatus status;
    int64_t     timestamp;   // Unix-style step counter
    std::string strategyTag; // Which strategy placed this (for logging)

    Order() = default;

    Order(const std::string& sym, OrderType t, OrderSide s,
          double px, int qty, int64_t ts, const std::string& tag = "")
        : symbol(sym), type(t), side(s), price(px),
          quantity(qty), filledQty(0), status(OrderStatus::PENDING),
          timestamp(ts), strategyTag(tag) {
        // Assign globally unique id via atomic counter
        static std::atomic<uint64_t> counter{1};
        id = counter.fetch_add(1, std::memory_order_relaxed);
    }

    int remainingQty() const { return quantity - filledQty; }
    bool isFilled()    const { return status == OrderStatus::FILLED; }
    bool isActive()    const {
        return status == OrderStatus::PENDING ||
               status == OrderStatus::PARTIALLY_FILLED;
    }

    std::string sideStr() const { return side == OrderSide::BUY ? "BUY" : "SELL"; }
    std::string typeStr() const { return type == OrderType::MARKET ? "MARKET" : "LIMIT"; }
};
