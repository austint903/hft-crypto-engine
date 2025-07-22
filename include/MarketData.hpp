#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>  
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cctype>


namespace ssl = boost::asio::ssl;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ws = beast::websocket;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

class MarketData{
    public:
        MarketData(asio::io_context& ioc, ssl::context& ssl_ctx, std::string host, std::string port, std::vector<std::string>& symbols);
        void run();
        void stop();

    private:
        void doResolve();
        // void onResolve(beast::error_code ec, tcp::resolver::results_type results);
        // void onConnect(beast::error_code ec, tcp::endpoint);
        // void onSslHandShake(beast::error_code ec);
        // void onWsHandshake(beast::error_code ec);
        // void onRead(beast::error_code ec, std::size_t bytes_transferred);
        std::string buildTarget() const;

        asio::io_context & ioc;
        tcp::resolver resolver;
        ws::stream<ssl::stream<tcp::socket>> ws;
        beast::flat_buffer buffer;
        std::string host;
        ssl::context& ssl_ctx;
        std::string port;
        std::vector<std::string> symbols;
        std::thread thread;
        std::atomic<bool> running{false};
};