#include <RiskManager.hpp>
#include <iostream>

RiskManager::RiskManager(OrderManager &om, double maxPositionPerSymbol, double maxTotalNotional)
    : om(om), maxPos(maxPositionPerSymbol), maxNotional(maxTotalNotional), notionalTraded(0.0)
{
    om.onOrderUpdate([this](const Order &ord)
                     { this->onOrderUpdate(ord); });
}

bool RiskManager::approve(const std::string &symbol, OrderSide side, double quantity, double price, std::string &reason)
{
    std::lock_guard lock(mu);
    double sign = (side == OrderSide::BUY ? +1.0 : -1.0);
    double newPos = positions[symbol] + sign * quantity;
    double orderNot = std::abs(quantity * price);
    double newNotion = notionalTraded + orderNot;
    if (std::abs(newPos) > maxPos)
    {
        reason = "position limit exceeded for " + symbol;
        return false;
    }
    if (newNotion > maxNotional)
    {
        reason = "total notional cap exceeded";
        return false;
    }
    return true;
}

void RiskManager::onOrderUpdate(const Order &ord)
{
    if (ord.status == OrderStatus::FILLED || ord.status == OrderStatus::PARTIAL)
    {
        std::lock_guard lock(mu);
        double sign = (ord.side == OrderSide::BUY ? +1.0 : -1.0);

        positions[ord.symbol] += sign * ord.lastFillQuantity;
        notionalTraded += std::abs(ord.lastFillQuantity * ord.lastFillPrice);

        std::cout << "RiskManager " << ord.clientId << " fill " << ord.lastFillQuantity << " at " << ord.lastFillPrice << " position in " << ord.symbol << ": " << positions[ord.symbol]
                  << ", total notional: " << notionalTraded << "\n";
    }
}