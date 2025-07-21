#include <MarketData.hpp>
#include <iostream>

MarketData::MarketData(asio::io_context& ioc, std::vector<std::string>& symbols):
    ioc(ioc), resolver(ioc), ws(ioc), symbols(std::move(symbols)){}

void MarketData::run(){
    if(running){
        return;
    }
    running=true;
    thread=std::thread([this] {
        doConnect();
        ioc.run();
    });
}
void MarketData::stop(){
    if(!running)return;
    running=false;
    beast::error_code ec;
    ws.close(ws::close_code::normal, ec);
    ioc.stop();
    if(thread.joinable()){
        thread.join();
    }
}

void MarketData::doConnect(){

}