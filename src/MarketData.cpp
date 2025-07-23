#include <MarketData.hpp>
#include <iostream>
#include <algorithm>
#include <exception>


MarketData::MarketData(asio::io_context& ioc, ssl::context& ssl_ctx, std::string host, std::string port, std::vector<std::string>& symbols):
    ioc(ioc), ssl_ctx(ssl_ctx),resolver(asio::make_strand(ioc)),ws(asio::make_strand(ioc), ssl_ctx), host(std::move(host)), port (std::move(port)), symbols(std::move(symbols)){}

void MarketData::run(){
    if(running){
        return;
    }
    running=true;
    thread=std::thread([this] {
        this->doResolve();
        ioc.run();
    });
}
void MarketData::stop(){
    if(!running)return;
    running=false;
    beast::error_code ec;
    ws.close(beast::websocket::close_code::normal, ec);
    //stop ssl
    ws.next_layer().shutdown(ec);
    //shutdown tcp
    ws.next_layer().next_layer().shutdown(tcp::socket::shutdown_both, ec);
    
    ioc.stop();
    if (thread.joinable()){
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
    resolver.async_resolve(
        host,port,beast::bind_front_handler(&MarketData::onResolve, this)
    );
}


void MarketData::onResolve(beast::error_code ec, tcp::resolver::results_type results){
    if(ec){
        std::cerr<<"resolve error "<<ec.message()<<"\n";
        return;
    }
    //starts TCP connection
    asio::async_connect(
        ws.next_layer().next_layer(), results, beast::bind_front_handler(&MarketData::onConnect, this)
    );

}

void MarketData::onConnect(beast::error_code ec, tcp::endpoint){
    if (ec) {
        std::cerr << "Connect error: " << ec.message() << "\n";
        return;
    }
    //TLS handshake
    ws.next_layer().async_handshake(
        ssl::stream_base::client, beast::bind_front_handler(&MarketData::onSslHandShake, this)
    );

}

void MarketData::onSslHandShake(beast::error_code ec){
    if(ec){
        std::cerr<<"ssl handshake error " <<ec.message()<<"\n";
        return;
    }
    std::string hostHeader = host + ":"+port;
    std::string target=buildTarget();
    //websocket handshake
    ws.async_handshake(hostHeader, target, beast::bind_front_handler(&MarketData::onWsHandshake, this));
}

void MarketData::onWsHandshake(beast::error_code ec){
    if(ec){
        std::cerr<<"ws handshake error " <<ec.message()<<"\n";
        return;
    }
    //after websocket handshake, start reading
    ws.async_read(buffer, beast::bind_front_handler(&MarketData::onRead, this));
}

void MarketData::onRead(beast::error_code ec, std::size_t bytes_transferred){
    if(ec){
        std::cerr<<"Read error "<<ec.message()<<"\n";
        return;
    }
    if(!running){
        return;
    }
    std::string payload=beast::buffers_to_string(buffer.data());
    buffer.consume(buffer.size());
    try{
        auto parseJson = json::parse(payload);
        auto stream = parseJson["stream"].get<std::string>();
        auto data=parseJson["data"];
        auto sym = stream.substr(0, stream.find('@'));
        std::cout << "\n=== " << sym << "\n";
        
        //top 5 bids
        std::cout<<"Bids"<<"\n";
        auto bids = data["b"];
        for(int i =0;i<std::min(5, (int) bids.size());++i){
            auto bid = bids[i];
            std::string price = bid[0].get<std::string>();
            std::string quantity = bid[1].get<std::string>();
            std::cout<<"Price: "<<price<<" |"<<" Quantity: "<<quantity<<"\n";
        }

        //top 5 asks
        std::cout<< "Asks"<<"\n";
        auto asks=data["a"];
        for(int i =0; i<std::min(5, (int) asks.size());++i){
            auto ask=asks[i];
            std::string price = ask[0].get<std::string>();
            std::string quantity = ask[1].get<std::string>();
            std::cout<<"Price: "<<price<<" |"<<" Quantity: "<<quantity<<"\n";
        }

    }catch(const std::exception& e){
        std::cerr<<"Parse/print error"<<e.what()<<"\n";
    }
    //cotninue reading after getting first batch
    ws.async_read(buffer, beast::bind_front_handler(&MarketData::onRead, this));

}




