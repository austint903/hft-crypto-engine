#include <PairsMeanReversionStrategy.hpp>
#include <cmath>
#include <limits>

PairsMeanReversionStrategy::PairsMeanReversionStrategy(OrderManager& om, std::string symbolA, std::string symbolB, double beta,size_t window, double entryZ, double exitZ)
    :om(om), stats(window), symbolA(std::move(symbolA)), symbolB(std::move(symbolB)), beta(beta), entryZ(entryZ), exitZ(exitZ), 
    lastPriceA(std::numeric_limits<double>::quiet_NaN()), lastPriceB(std::numeric_limits<double>::quiet_NaN()){}


void PairsMeanReversionStrategy:: onMarketData (const MarketData::Update& update){
    if(update.symbol==symbolA){
        lastPriceA=update.midPrice();
    }
    else if ( update.symbol == symbolB){
        lastPriceB = update.midPrice();
    }
    else{
        return;
    }
    if(!std::isnan(lastPriceA) && !std::isnan(lastPriceB)){
        double spread = lastPriceA - beta * lastPriceB;
        stats.add(spread);
        if(stats.ready()){
            double mean = stats.mean();
            double sigma = stats.stddev();
            if(sigma>0.0){
                //calculate z-score
                double z = (spread - mean)/sigma;
                //call signal

            }
        }
    }

}

void PairsMeanReversionStrategy::generateSignals(double z, double priceA, double priceB){
    if(z<-entryZ){
        //long spread, buy A, sell B
    }
    else if (z>entryZ){
        //short spread sell A, buy B
    }
}


void PairsMeanReversionStrategy::attemptTrade(const std::string& sym, OrderSide side, double qty, double price){
    Order o;
    //add a symbol field for order struct in the future so that risk manager knows
    o.clientId="";
    o.side=side;
    o.quantity=qty;
    o.price=price;
    o.status=OrderStatus::NEW;
    //add risk manager here
    om.sendOrder(side, qty, price, sym);
}