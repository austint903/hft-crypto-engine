#pragma once

#include "OrderManager.hpp"
#include <mutex>
#include <unordered_map>
#include <string>

class RiskManager
{
public:
    RiskManager(OrderManager &om, double maxPositionPerSymbol, double maxTotalNotional);
    ~RiskManager() = default;

    bool approve(const std::string &symbol, OrderSide side, double quantity, double price, std::string &reason);

private:
    void onOrderUpdate(const Order &order);
    OrderManager &om;

    double maxPos;
    double maxNotional;

    std::mutex mu;
    std::unordered_map<std::string, double> positions;
    double notionalTraded;
};