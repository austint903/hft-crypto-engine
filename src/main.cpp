#include "MarketData.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <thread>

int main() {
    boost::asio::io_context ioc;
    std::vector<std::string>crypto= {"btcusdt", "ethusdt"};
    MarketData md(ioc, crypto);
    md.run();
    while(true){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    md.stop();
    return 0;
}