#pragma once

#include <MarketData.hpp>
#include <OrderManager.hpp>

//parent class for all strategies
class Strategy{
    public: 
        virtual ~Strategy()=default;
        //called everytime we have a new market update
        virtual void onMarketData (const MarketData::Update& upd)=0;
        //called on every order update(acknowlegde/fill)
        // virtual void onOrderUpdate(const Order& ord)=0;
};