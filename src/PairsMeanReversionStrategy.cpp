#include <PairsMeanReversionStrategy.hpp>
#include <cmath>
#include <limits>
#include <iostream>

PairsMeanReversionStrategy::PairsMeanReversionStrategy(OrderManager &om, RiskManager &rm, std::string symbolA, std::string symbolB, double beta, size_t window, double entryZ, double exitZ)
    : om(om), rm(rm), stats(window), symbolA(std::move(symbolA)), symbolB(std::move(symbolB)), beta(beta), entryZ(entryZ), exitZ(exitZ),
      lastPriceA(std::numeric_limits<double>::quiet_NaN()), lastPriceB(std::numeric_limits<double>::quiet_NaN()) {}

void PairsMeanReversionStrategy::onMarketData(const MarketData::Update &update)
{
    if (update.symbol == symbolA)
    {
        lastPriceA = update.midPrice();
    }
    else if (update.symbol == symbolB)
    {
        lastPriceB = update.midPrice();
    }
    else
    {
        return;
    }
    if (!std::isnan(lastPriceA) && !std::isnan(lastPriceB))
    {
        double spread = lastPriceA - beta * lastPriceB;
        stats.add(spread);
        if (stats.ready())
        {
            double mean = stats.mean();
            double sigma = stats.stddev();
            if (sigma > 0.0)
            {
                // calculate z-score
                double z = (spread - mean) / sigma;
                // call signal
                generateSignals(z, lastPriceA, lastPriceB);
            }
        }
    }
}

void PairsMeanReversionStrategy::generateSignals(double z, double priceA, double priceB)
{
    if (z < -entryZ)
    {
        // long spread, buy A, sell B
        attemptTrade(symbolA, OrderSide::BUY, 1.0, priceA);
        attemptTrade(symbolB, OrderSide::SELL, beta, priceB);
    }
    else if (z > entryZ)
    {
        // short spread sell A, buy B
        attemptTrade(symbolA, OrderSide::SELL, 1.0, priceA);
        attemptTrade(symbolB, OrderSide::BUY, beta, priceB);
    }
}

void PairsMeanReversionStrategy::attemptTrade(const std::string &sym, OrderSide side, double qty, double price)
{
    Order o;
    // add a symbol field for order struct in the future so that risk manager knows
    o.clientId = "";
    o.side = side;
    o.quantity = qty;
    o.price = price;
    o.status = OrderStatus::NEW;
    o.symbol=sym;
    // add risk manager here
    std::string reason;
    if (rm.approve(sym, side, qty, price, reason))
    {
        om.sendOrder(side, qty, price, sym);
    }
    else
    {
        std::cerr << "risk rejected: " << reason << "\n";
    }
}