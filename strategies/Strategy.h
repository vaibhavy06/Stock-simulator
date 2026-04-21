#pragma once
#include <string>
#include <vector>
#include <optional>
#include "../models/Order.h"

// Signal emitted by a strategy
struct Signal {
    std::string symbol;
    OrderSide   side;
    int         quantity;
    double      price;       // 0.0 = market order
    std::string reason;      // Human-readable rationale

    bool isMarket() const { return price == 0.0; }
};

// Abstract Strategy base class (Strategy Pattern + Open/Closed Principle)
class Strategy {
public:
    explicit Strategy(const std::string& name) : name_(name) {}
    virtual ~Strategy() = default;

    // Called each time step with the latest price for a symbol.
    // Returns an optional signal if the strategy wants to act.
    virtual std::optional<Signal> onPrice(const std::string& symbol,
                                          double price,
                                          int64_t timestamp) = 0;

    // Reset internal state (for backtesting across multiple runs)
    virtual void reset() = 0;

    const std::string& name() const { return name_; }

protected:
    std::string name_;
};
