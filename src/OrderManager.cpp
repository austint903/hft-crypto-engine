#include <OrderManager.hpp>
#include <iostream>
#include <exception>

OrderManager::OrderManager(asio::io_context &ioc, ssl::context &ssl_ctx, std::string host, std::string port) 
    : ioc(ioc), ssl_ctx(ssl_ctx), resolver(asio::make_strand(ioc)), ws(asio::make_strand(ioc), ssl_ctx), 
      strand(ioc.get_executor()), host(std::move(host)), port(std::move(port)) {}

OrderManager::~OrderManager() 
{
    stop();
}

void OrderManager::run()
{
    if (running)
        return;
    running = true;
    thread = std::thread([this]
                         {
        this->doResolve();
        ioc.run(); });
}

void OrderManager::stop()
{
    if (!running)
        return;
    
    running = false;
    
    boost::asio::post(strand, [this]() {
        beast::error_code ec;
        
        if (ws.is_open()) {
            ws.async_close(beast::websocket::close_code::normal, 
                [this](beast::error_code ec) {
                    if (ec) {
                        std::cerr << "WebSocket close error: " << ec.message() << "\n";
                    }
                    
                    beast::error_code ssl_ec;
                    ws.next_layer().async_shutdown([this](beast::error_code ec) {
                        if (ec && ec != asio::error::eof) {
                            std::cerr << "SSL shutdown error: " << ec.message() << "\n";
                        }
                        
                        beast::error_code tcp_ec;
                        ws.next_layer().next_layer().shutdown(tcp::socket::shutdown_both, tcp_ec);
                        if (tcp_ec && tcp_ec != asio::error::not_connected) {
                            std::cerr << "TCP shutdown error: " << tcp_ec.message() << "\n";
                        }
                        
                        ioc.stop();
                    });
                });
        } else {
            ioc.stop();
        }
    });
    
    if (thread.joinable()) {
        thread.join();
    }
}

void OrderManager::doResolve()
{
    resolver.async_resolve(
        host, port, beast::bind_front_handler(&OrderManager::onResolve, this));
}

void OrderManager::onResolve(beast::error_code ec, tcp::resolver::results_type results)
{
    if (ec)
    {
        std::cerr << "Resolve error: " << ec.message() << "\n";
        return;
    }
    
    asio::async_connect(
        ws.next_layer().next_layer(), results, 
        beast::bind_front_handler(&OrderManager::onConnect, this));
}

void OrderManager::onConnect(beast::error_code ec, tcp::endpoint)
{
    if (ec)
    {
        std::cerr << "Connect error: " << ec.message() << "\n";
        return;
    }
    
    if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
        std::cerr << "Failed to set SNI hostname\n";
    }
    
    ws.next_layer().async_handshake(
        ssl::stream_base::client, 
        beast::bind_front_handler(&OrderManager::onSslHandShake, this));
}

void OrderManager::onSslHandShake(beast::error_code ec)
{
    if (ec)
    {
        std::cerr << "SSL handshake error: " << ec.message() << "\n";
        return;
    }
    
    ws.set_option(beast::websocket::stream_base::timeout::suggested(
        beast::role_type::client));
    
    ws.set_option(beast::websocket::stream_base::decorator(
        [](beast::websocket::request_type& req)
        {
            req.set(beast::http::field::user_agent, 
                std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl");
        }));
    
    const std::string target = "/ws-api/v3";  
    const std::string hostHeader = host + ":" + port;
    
    ws.async_handshake(hostHeader, target, 
        beast::bind_front_handler(&OrderManager::onWsHandshake, this));
}

void OrderManager::onWsHandshake(beast::error_code ec)
{
    if (ec)
    {
        std::cerr << "WebSocket handshake error: " << ec.message() << "\n";
        return;
    }
    
    std::cout << "OrderManager WebSocket connected successfully\n";
    
    ws.async_read(buffer, beast::bind_front_handler(&OrderManager::onRead, this));
}

void OrderManager::onRead(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec)
    {
        std::cerr << "Read error: " << ec.message() << "\n";
        return;
    }
    
    if (!running)
    {
        return;
    }

    try
    {
        std::string payload = beast::buffers_to_string(buffer.data());
        buffer.consume(buffer.size());

        std::cout << "Received: " << payload << "\n";

        auto json_response = nlohmann::json::parse(payload);

        if (json_response.contains("id"))
        {
            std::string clientId = json_response["id"];
            
            if (json_response.contains("code")) {
                std::cerr << "Order error - Code: " << json_response["code"] 
                         << ", Message: " << json_response.value("msg", "Unknown error") << "\n";
                handleExchangeAcknowledge(clientId, 0.0, 0.0, false);
            } else {
                double fillQty = 0.0;
                double fillPrice = 0.0;
                bool success = true;
                
                if (json_response.contains("executedQty")) {
                    fillQty = std::stod(json_response["executedQty"].get<std::string>());
                }
                if (json_response.contains("price")) {
                    fillPrice = std::stod(json_response["price"].get<std::string>());
                }
                
                std::string status = json_response.value("status", "");
                success = (status != "REJECTED");
                
                handleExchangeAcknowledge(clientId, fillQty, fillPrice, success);
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing order response: " << e.what() << "\n";
    }

    ws.async_read(buffer, beast::bind_front_handler(&OrderManager::onRead, this));
}

std::string OrderManager::sendOrder(OrderSide side, double quantity, double price, const std::string &symbol)
{
    std::string id = "client_" + std::to_string(nextId.fetch_add(1));
    
    Order order;
    order.symbol = symbol;
    order.clientId = id;
    order.side = side;
    order.price = price;
    order.quantity = quantity;
    order.status = OrderStatus::NEW;

    {
        std::lock_guard lock(mu);
        orders.emplace(id, order);
    }

    nlohmann::json j;
    j["id"] = id;
    j["method"] = "order.place";
    j["params"] = {
        {"symbol", symbol},
        {"side", (side == OrderSide::BUY ? "BUY" : "SELL")},
        {"type", "LIMIT"},
        {"timeInForce", "GTC"},
        {"quantity", std::to_string(quantity)},
        {"price", std::to_string(price)},
        {"newClientOrderId", id}
    };

    auto message = j.dump();
    
    std::cout << "Sending order: " << message << "\n";

    boost::asio::post(strand, [this, msg = std::move(message)]() { 
        doSend(msg); 
    });

    return id;
}

void OrderManager::cancelOrder(const std::string &clientId)
{
    nlohmann::json j;
    j["id"] = clientId + "_cancel";
    j["method"] = "order.cancel";
    j["params"] = {
        {"origClientOrderId", clientId}
    };
    
    auto message = j.dump();
    
    boost::asio::post(strand, [this, msg = std::move(message)]() { 
        doSend(msg); 
    });
}

void OrderManager::onOrderUpdate(OrderCallback cb)
{
    callback = std::move(cb);
}

void OrderManager::handleExchangeAcknowledge(const std::string &clientId, double filledQuantity, double filledPrice, bool success)
{
    Order updated;
    {
        std::lock_guard lock(mu);
        auto it = orders.find(clientId);
        if (it == orders.end())
        {
            std::cerr << "Order not found in order book: " << clientId << "\n";
            return;
        }
        
        Order &origOrder = it->second;
        double originalQty = origOrder.quantity;

        updated = origOrder;
        updated.lastFillQuantity = filledQuantity;
        updated.lastFillPrice = filledPrice;

        if (!success)
        {
            updated.status = OrderStatus::REJECTED;
        }
        else if (filledQuantity == 0.0)
        {
            updated.status = OrderStatus::ACKED;
        }
        else if (filledQuantity < originalQty)
        {
            updated.status = OrderStatus::PARTIAL;
            updated.quantity = originalQty - filledQuantity;
        }
        else
        {
            updated.status = OrderStatus::FILLED;
            updated.quantity = 0.0;
        }
        
        it->second = updated;
    }
    
    if (callback)
    {
        callback(updated);
    }
}

void OrderManager::doSend(const std::string &payload)
{
    if (!ws.is_open()) {
        std::cerr << "WebSocket is not open, cannot send message\n";
        return;
    }
    
    ws.async_write(
        asio::buffer(payload),
        asio::bind_executor(strand, [this, payload](beast::error_code ec, std::size_t bytes_written)
        {
            if (ec) {
                std::cerr << "Send error: " << ec.message() << "\n";
                std::cerr << "Failed to send: " << payload << "\n";
                return;
            }
            std::cout << "Successfully sent " << bytes_written << " bytes\n";
        }));
}
