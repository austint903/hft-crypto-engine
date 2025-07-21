#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ws = beast::websocket;
using tcp = asio::ip::tcp;

class MarketData{
    public:
        MarketData(asio::io_context& ioc, std::vector<std::string>& symbols);
        void run();
        void stop();

    private:
        void doConnect();
        void onRead(beast::error_code ec, std::size_t bytes_transferred);
        std::string buildTarget() const;

        asio::io_context & ioc;
        tcp::resolver resolver;
        ws::stream<tcp::socket> ws;
        beast::flat_buffer buffer;

        std::vector<std::string> symbols;
        std::thread thread;
        std::atomic<bool> running{false};
};