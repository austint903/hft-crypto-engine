#include <OrderManager.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

OrderManager::OrderManager(boost::asio::io_context& ioc):ioc(ioc), strand(ioc.get_executor()){}

OrderManager::~OrderManager() = default;

std::string OrderManager::sendOrder(OrderSide side, double quantity, double price, const std::string& symbol){
    std::string id = "client " + std::to_string(nextId.fetch_add(1));
    //order struct
    Order order;
    order.clientId=id;
    order.side=side;
    order.price=price;
    order.quantity=quantity;
    order.status=OrderStatus::NEW;

    //lock mutex until emplace
    {
        std::lock_guard lock(mu);
        orders.emplace(id, order);
    }

    //build object
    nlohmann::json j;
    j["id"] = id;
    j["symbol"]=symbol;
    j["side"] = (side==OrderSide::BUY ? "BUY" :"SELL");
    j["quantity"] = quantity;
    j["price"] = price;
    j["type"] = "LIMIT";

    auto message = j.dump();

    boost::asio::post(strand, [this, msg=std::move(message)]{
        doSend(msg);
    });

    return id;
}
void OrderManager::cancelOrder(const std::string& clientId){
    nlohmann::json j;
    j["id"] = clientId;
    j["action"] = "CANCEL";
    auto message = j.dump();
    boost::asio::post(strand, [this, msg=std::move(message)]{
        doSend(msg);
    });
}

void OrderManager::onOrderUpdate(OrderCallback cb){
    callback = std::move(cb);
}
void OrderManager::handleExchangeAcknowledge(const std::string& clientId, double filledQuantity, double filledPrice, bool success){
    Order updated;
    {
        std::lock_guard lock(mu);
        auto it = orders.find(clientId);
        if(it==orders.end()){
            std::cerr<<"order is not in the order book"<<"\n";
            return;
        }
        Order& origOrder = it->second;
        double originalQty = origOrder.quantity;

        updated = origOrder;
        updated.lastFillQuantity = filledQuantity;
        updated.lastFillPrice    = filledPrice;

        if (!success) {
            updated.status = OrderStatus::REJECTED;
        }
        else if (filledQuantity == 0.0) {
            updated.status = OrderStatus::ACKED;
        }
        else if (filledQuantity < originalQty) {
            updated.status   = OrderStatus::PARTIAL;
            updated.quantity = originalQty - filledQuantity;
        }
        else {
            updated.status   = OrderStatus::FILLED;
            updated.quantity = 0.0;
        }
        it->second = updated;
    }
    //notifies strategy
    if(callback){
        callback(updated);
    }
}   
void OrderManager::doSend(const std::string& payload){

}