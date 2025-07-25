#pragma once

#include <boost/asio.hpp>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <functional>


enum class OrderSide{BUY, SELL};
enum class OrderStatus { NEW, ACKED, PARTIAL, FILLED, CANCELED, REJECTED };

struct Order{
    std::string clientId;
    OrderSide side;
    double quantity;
    double price;
    OrderStatus status;

    //for partial fills
    double lastFillQuantity=0.0;
    double lastFillPrice=0.0;
};

class OrderManager{
    //type aliasing for any callable that takes in order
    using OrderCallback = std::function<void(const Order&)>;
    public:
        explicit OrderManager(boost::asio::io_context& ioc);
        ~OrderManager();
        //returns client Id
        std::string sendOrder(OrderSide side, double quantity, double price, const std::string& symbol);
        // void cancelOrder(const std::string& clientId);

        //gets the update order object after some status
        void onOrderUpdate(OrderCallback cb);
        
        void cancelOrder(const std::string& clientId);
        void handleExchangeAcknowledge(const std::string& clientId, double filledQuantity, double filledPrice, bool success);



    private:    
        void doSend(const std::string& payload);
        boost::asio::io_context& ioc;
        //help with concurrency (in order)/ thread safety
        boost::asio::strand<boost::asio::io_context::executor_type> strand;

        std::mutex mu;  
        //eliminates race conditions, unique id
        std::atomic<uint64_t> nextId{1};

        //id to order mapping
        std::unordered_map<std::string, Order> orders;

        //registered callback
        OrderCallback callback;
};
