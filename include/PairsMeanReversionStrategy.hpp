#pragma once

#include <Strategy.hpp>
#include <RollingStats.hpp>
#include <string>
#include <limits>
#include "OrderManager.hpp"
#include <RiskManager.hpp>

// mean reversion with pairs trading
//  - calculate spread
//  - calculate z score from spread
//  - enter/exit based on z threshold
class PairsMeanReversionStrategy : public Strategy
{
public:
    PairsMeanReversionStrategy(OrderManager &om, RiskManager &rm, std::string symbolA, std::string symbolB, double beta, size_t window, double entryZ, double exitZ);
    void onMarketData(const MarketData::Update &upd) override;
    // void onOrderUpdate(const Order& ord)override;

private:
    void generateSignals(double z, double priceA, double priceB);
    void attemptTrade(const std::string &sym, OrderSide side, double qty, double price);
    OrderManager &om;
    RollingStats stats;
    RiskManager &rm;

    std::string symbolA, symbolB;
    double beta; // hedge ratio
    double entryZ, exitZ;

    double lastPriceA, lastPriceB;
};