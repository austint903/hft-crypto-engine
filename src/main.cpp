#include "MarketData.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include <string>

int main() {
    asio::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);
    std::vector<std::string>symbols={"btcusdt","ethusdt"};
    MarketData md(ioc, ctx, "fstream.binance.com", "443", symbols);
    md.run();

    while(true){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}