#include "MarketData.hpp"
#include "OrderManager.hpp"
#include "PairsMeanReversionStrategy.hpp"
#include "RiskManager.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

int main()
{
    asio::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);

    std::vector<std::string> symbols = {"btcusdt", "ethusdt"};

    MarketData md(ioc, ctx, "fstream.binance.com", "443", symbols);

    //
    OrderManager om(ioc, ctx, "testnet.binance.vision", "443"); 

    RiskManager rm(om, 5.0, 500000.0);
    PairsMeanReversionStrategy strategy(om, rm, "BTCUSDT", "ETHUSDT", 0.065, 20, 2.0, 0.5);

    om.onOrderUpdate([](const Order &order)
                     {
        std::cout << "=== ORDER UPDATE ===" << std::endl;
        std::cout << "ID: " << order.clientId << std::endl;
        std::cout << "Symbol: " << order.symbol << std::endl;
        std::cout << "Side: " << (order.side == OrderSide::BUY ? "BUY" : "SELL") << std::endl;
        std::cout << "Quantity: " << order.quantity << std::endl;
        std::cout << "Price: " << std::fixed << std::setprecision(4) << order.price << std::endl;
        std::cout << "Status: ";
        switch(order.status) {
            case OrderStatus::NEW: std::cout << "NEW"; break;
            case OrderStatus::ACKED: std::cout << "ACKNOWLEDGED"; break;
            case OrderStatus::PARTIAL: std::cout << "PARTIALLY_FILLED"; break;
            case OrderStatus::FILLED: std::cout << "FILLED"; break;
            case OrderStatus::CANCELED: std::cout<<"CANCELED"; break;
            case OrderStatus::REJECTED: std::cout << "REJECTED"; break;
        }
        std::cout<<std::endl;
        if (order.lastFillQuantity>0) {
            std::cout <<"Last fill " << order.lastFillQuantity <<"at" << std::fixed << std::setprecision(4)<<order.lastFillPrice << std::endl;
        }
        std::cout << "===================" << std::endl << std::endl; });

    md.onUpdate([&strategy](const MarketData::Update &update)
                {
        //log every 5th update
        static int counter = 0;
        if (++counter % 5 == 0) {
            std::cout << "=== MARKET DATA ===" << std::endl;
            std::cout << "Symbol: " << update.symbol << std::endl;
            std::cout << "Mid Price: " << std::fixed << std::setprecision(4) << update.midPrice() << std::endl;
            
            if (!update.bids.empty() && !update.asks.empty()) {
                std::cout << "Best Bid: " << update.bids[0].price << " (" << update.bids[0].quantity << ")" << std::endl;
                std::cout << "Best Ask: " << update.asks[0].price  << " (" << update.asks[0].quantity << ")" << std::endl;
                std::cout << "Spread: " << std::fixed << std::setprecision(4) << (update.asks[0].price - update.bids[0].price) << std::endl;
            }
            std::cout << "===================" << std::endl << std::endl;
        }
        //get signal
        strategy.onMarketData(update); });

    std::cout << "Starting!" << std::endl;
    std::cout << "risk limits: 5 units per crypto and 500k total notional" << std::endl;

    md.run();
    om.run();

    //display status
    int secondsRunning = 0;
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        secondsRunning++;

        if (secondsRunning % 5 == 0)
        {
            std::cout << "--- Status: Running for " << secondsRunning << " seconds ---" << std::endl;
        }
    }
}