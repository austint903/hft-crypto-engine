#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp> 
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cctype>
#include <thread>



enum class OrderSide{BUY, SELL};
enum class OrderStatus { NEW, ACKED, PARTIAL, FILLED, CANCELED, REJECTED };

namespace ssl = boost::asio::ssl;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ws = beast::websocket;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

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
        explicit OrderManager(asio::io_context& ioc,ssl::context& sll_ctx, std::string& host, std::string& port);
        ~OrderManager();
        //returns client Id
        std::string sendOrder(OrderSide side, double quantity, double price, const std::string& symbol);
        // void cancelOrder(const std::string& clientId);

        //gets the update order object after some status
        void onOrderUpdate(OrderCallback cb);
        
        void cancelOrder(const std::string& clientId);
        void handleExchangeAcknowledge(const std::string& clientId, double filledQuantity, double filledPrice, bool success);
        void run();
        void stop();



    private:    



        void doSend(const std::string& payload);
        asio::io_context& ioc;

        //help with concurrency (in order)/ thread safety
        asio::strand<boost::asio::io_context::executor_type> strand;

        std::mutex mu;  
        //eliminates race conditions, unique id
        std::atomic<uint64_t> nextId{1};

        //id to order mapping
        std::unordered_map<std::string, Order> orders;

        //registered callback
        OrderCallback callback;

        //websocket
        void doResolve();
        void onResolve(beast::error_code ec, tcp::resolver::results_type results);
        void onConnect(beast::error_code ec, tcp::endpoint);
        void onSslHandShake(beast::error_code ec);
        void onWsHandshake(beast::error_code ec);
        void onRead(beast::error_code ec, std::size_t bytes_transferred);
        ssl::context& ssl_ctx;
        tcp::resolver resolver;
        ws::stream<ssl::stream<tcp::socket>> ws;
        beast::flat_buffer buffer;
        std::string port;
        std::string host;
        std::thread thread;
        std::atomic<bool> running{false};
};
