#include <MarketData.hpp>
#include <iostream>


MarketData::MarketData(asio::io_context& ioc, ssl::context& ssl_ctx, std::string host, std::string port, std::vector<std::string>& symbols):
    ioc(ioc), ssl_ctx(ssl_ctx),resolver(asio::make_strand(ioc)),ws(asio::make_strand(ioc), ssl_ctx), host(std::move(host)), port (std::move(port)), symbols(std::move(symbols)){}

void MarketData::run(){
    if(running){
        return;
    }
    running=true;
    thread=std::thread([this] {
        doResolve();
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


std::string MarketData::buildTarget() const
{
    if(symbols.empty()){
        return "/ws/btcusdt@depth5";
    }
    std::string target="/stream?streams=";
    bool first=true;
    for(const auto & symbol:symbols){
        if(!first) target+="/";
        std::string lowerSymbol = symbol;
        std::transform(lowerSymbol.begin(), lowerSymbol.end(), 
                      lowerSymbol.begin(), ::tolower);
        
        target += lowerSymbol + "@depth5";
        first = false;
    }
    return target;
}
void MarketData::doResolve(){

}