#include <OrderManager.hpp>
#include <iostream>
#include <exception>

OrderManager::OrderManager(asio::io_context& ioc, ssl::context& ssl_ctx,  std::string& host, std::string& port):ioc(ioc), ssl_ctx(ssl_ctx), resolver(asio::make_strand(ioc)), ws(asio::make_strand(ioc), ssl_ctx), strand(ioc.get_executor()) ,host(std::move(host)), port(std::move(port)){}

OrderManager::~OrderManager() = default;

void OrderManager::run() {
    if(running)return;
    running=true;
    thread=std::thread([this]{
        this->doResolve();
        ioc.run();
    });
}

void OrderManager::stop(){
    if(!running)return;
    running=false;
    beast::error_code ec;
    ws.close(beast::websocket::close_code::normal, ec);
    //stop sll
    ws.next_layer().shutdown(ec);
    ws.next_layer().next_layer().shutdown(tcp::socket::shutdown_both,ec);
    ioc.stop();
    if(thread.joinable()){
        thread.join();
    }
}
void OrderManager::doResolve(){
    resolver.async_resolve(
        host, port, beast::bind_front_handler(&OrderManager::onResolve, this)
    );
}

void OrderManager::onResolve(beast::error_code ec, tcp::resolver::results_type results){
    if(ec){
        std::cerr<<"resolve error "<<ec.message()<<"\n";
        return;
    }
    //start Tcp conneciton
    asio::async_connect(
        ws.next_layer().next_layer(), results, beast::bind_front_handler(&OrderManager::onConnect, this)
    );
}

void OrderManager::onConnect(beast::error_code ec, tcp::endpoint){
    if (ec) {
        std::cerr << "Connect error: " << ec.message() << "\n";
        return;
    }
    //TLS handshake
    ws.next_layer().async_handshake(
        ssl::stream_base::client, beast::bind_front_handler(&OrderManager::onSslHandShake, this)
    );

}

void OrderManager::onSslHandShake(beast::error_code ec){
    if(ec){
        std::cerr<<"ssl handshake error "<<ec.message()<<"\n";
        return;
    }
    const std::string target ="/ws";
    const std::string hostHeader = host + ":" +port;
    ws.async_handshake(hostHeader, target,beast::bind_front_handler(&OrderManager::onWsHandshake, this) );
}

void OrderManager::onWsHandshake(beast::error_code ec){
    if(ec){
        std::cerr<<"ws handshake error "<<ec.message()<<"\n";
        return;
    }
    ws.async_read(buffer, beast::bind_front_handler(&OrderManager::onRead, this));
}

void OrderManager::onRead(beast::error_code ec, std::size_t){
    if(ec){
        std::cerr<<"read error "<<ec.message()<<"\n";
        return;
    }
    if(!running){
        return;
    }
    try{
        std::cout<<"Running"<<"\n";
    }catch (const std::exception& e){
        std::cerr<<"error "<<e.what()<<"\n";
    }
}


std::string OrderManager::sendOrder(OrderSide side, double quantity, double price, const std::string& symbol){
    std::string id = "client " + std::to_string(nextId.fetch_add(1));
    //order struct
    Order order;
    order.clientId=id;
    order.side=side;
    order.price=price;
    order.quantity=quantity;
    order.status=OrderStatus::NEW;

    //lock mutex until emplace
    {
        std::lock_guard lock(mu);
        orders.emplace(id, order);
    }

    //build object
    nlohmann::json j;
    j["id"] = id;
    j["symbol"]=symbol;
    j["side"] = (side==OrderSide::BUY ? "BUY" :"SELL");
    j["quantity"] = quantity;
    j["price"] = price;
    j["type"] = "LIMIT";

    auto message = j.dump();

    boost::asio::post(strand, [this, msg=std::move(message)]{
        doSend(msg);
    });

    return id;
}
void OrderManager::cancelOrder(const std::string& clientId){
    nlohmann::json j;
    j["id"] = clientId;
    j["action"] = "CANCEL";
    auto message = j.dump();
    boost::asio::post(strand, [this, msg=std::move(message)]{
        doSend(msg);
    });
}

void OrderManager::onOrderUpdate(OrderCallback cb){
    callback = std::move(cb);
}

//consider removing each order from map once filled or rejected
void OrderManager::handleExchangeAcknowledge(const std::string& clientId, double filledQuantity, double filledPrice, bool success){
    Order updated;
    {
        std::lock_guard lock(mu);
        auto it = orders.find(clientId);
        if(it==orders.end()){
            std::cerr<<"order is not in the order book"<<"\n";
            return;
        }
        Order& origOrder = it->second;
        double originalQty = origOrder.quantity;

        updated = origOrder;
        updated.lastFillQuantity = filledQuantity;
        updated.lastFillPrice    = filledPrice;

        if (!success) {
            updated.status = OrderStatus::REJECTED;
        }
        else if (filledQuantity == 0.0) {
            updated.status = OrderStatus::ACKED;
        }
        else if (filledQuantity < originalQty) {
            updated.status   = OrderStatus::PARTIAL;
            updated.quantity = originalQty - filledQuantity;
        }
        else {
            updated.status   = OrderStatus::FILLED;
            updated.quantity = 0.0;
        }
        it->second = updated;
    }
    //notifies strategy
    if(callback){
        callback(updated);
    }
}   

//add error handling for bind_executor
void OrderManager::doSend(const std::string& payload){
    ws.async_write(asio::buffer(payload), asio::bind_executor(strand));
}