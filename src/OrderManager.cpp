#include <OrderManager.hpp>
#include <nlohmann/json.hpp>

OrderManager::OrderManager(boost::asio::io_context& ioc):ioc(ioc), strand(ioc.get_executor()){}

OrderManager::~OrderManager() = default;

std::string OrderManager::sendOrder(OrderSide side, double quantity, double price){
    std::string id = "client " + std::to_string(nextId.fetch_add(1));
    //order struct
    Order order;
    order.clientId=id;
    order.side=side;
    order.quantity=quantity;
    order.status=OrderStatus::NEW;

    orders.emplace(id, order);

    
    return id;
}

void OrderManager::onOrderUpdate(OrderCallback cb){
    callback = std::move(cb);
}
