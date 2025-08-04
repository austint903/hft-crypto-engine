#include <MarketData.hpp>
#include <iostream>
#include <algorithm>
#include <exception>
#include <string>

MarketData::MarketData(asio::io_context &ioc, ssl::context &ssl_ctx, std::string host, std::string port, std::vector<std::string> &symbols) : ioc(ioc), ssl_ctx(ssl_ctx), resolver(asio::make_strand(ioc)), ws(asio::make_strand(ioc), ssl_ctx),
                                                                                                                                              host(std::move(host)), port(std::move(port)), symbols(std::move(symbols)) {}

void MarketData::run()
{
    if (running)
    {
        return;
    }
    running = true;
    thread = std::thread([this]
                         {
        this->doResolve();
        ioc.run(); });
}

void MarketData::stop()
{
    if (!running)
        return;
    running = false;
    beast::error_code ec;
    ws.close(beast::websocket::close_code::normal, ec);
    // stop ssl
    ws.next_layer().shutdown(ec);
    // shutdown tcp
    ws.next_layer().next_layer().shutdown(tcp::socket::shutdown_both, ec);

    ioc.stop();
    if (thread.joinable())
    {
        thread.join();
    }
}

void MarketData::onUpdate(std::function<void(const Update &)> cb)
{
    updateCallback = std::move(cb);
}

std::string MarketData::buildTarget() const
{
    if (symbols.empty())
    {
        return "/ws/btcusdt@depth5";
    }
    std::string target = "/stream?streams=";
    bool first = true;
    for (const auto &symbol : symbols)
    {
        if (!first)
            target += "/";
        std::string lowerSymbol = symbol;
        std::transform(lowerSymbol.begin(), lowerSymbol.end(),
                       lowerSymbol.begin(), ::tolower);

        target += lowerSymbol + "@depth5";
        first = false;
    }
    return target;
}

void MarketData::doResolve()
{
    resolver.async_resolve(
        host, port, beast::bind_front_handler(&MarketData::onResolve, this));
}

void MarketData::onResolve(beast::error_code ec, tcp::resolver::results_type results)
{
    if (ec)
    {
        std::cerr << "resolve error " << ec.message() << "\n";
        return;
    }
    // starts TCP connection
    asio::async_connect(
        ws.next_layer().next_layer(), results, beast::bind_front_handler(&MarketData::onConnect, this));
}

void MarketData::onConnect(beast::error_code ec, tcp::endpoint)
{
    if (ec)
    {
        std::cerr << "Connect error: " << ec.message() << "\n";
        return;
    }
    // TLS handshake
    ws.next_layer().async_handshake(
        ssl::stream_base::client, beast::bind_front_handler(&MarketData::onSslHandShake, this));
}

void MarketData::onSslHandShake(beast::error_code ec)
{
    if (ec)
    {
        std::cerr << "ssl handshake error " << ec.message() << "\n";
        return;
    }
    std::string hostHeader = host + ":" + port;
    std::string target = buildTarget();
    // websocket handshake
    ws.async_handshake(hostHeader, target, beast::bind_front_handler(&MarketData::onWsHandshake, this));
}

void MarketData::onWsHandshake(beast::error_code ec)
{
    if (ec)
    {
        std::cerr << "ws handshake error " << ec.message() << "\n";
        return;
    }
    // after websocket handshake, start reading
    ws.async_read(buffer, beast::bind_front_handler(&MarketData::onRead, this));
}

void MarketData::onRead(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec)
    {
        std::cerr << "Read error " << ec.message() << "\n";
        return;
    }
    if (!running)
    {
        return;
    }

    std::string payload = beast::buffers_to_string(buffer.data());
    buffer.consume(buffer.size());

    try
    {
        auto parseJson = json::parse(payload);
        auto stream = parseJson["stream"].get<std::string>();
        auto data = parseJson["data"];
        auto sym = stream.substr(0, stream.find('@'));

        std::transform(sym.begin(), sym.end(), sym.begin(), ::toupper);

        Update update;
        update.symbol = sym;

        auto bids = data["b"];
        for (int i = 0; i < std::min(5, (int)bids.size()); ++i)
        {
            auto bid = bids[i];
            Level level;
            level.price = std::stod(bid[0].get<std::string>());
            level.quantity = std::stod(bid[1].get<std::string>());
            update.bids.push_back(level);
        }

        auto asks = data["a"];
        for (int i = 0; i < std::min(5, (int)asks.size()); ++i)
        {
            auto ask = asks[i];
            Level level;
            level.price = std::stod(ask[0].get<std::string>());
            level.quantity = std::stod(ask[1].get<std::string>());
            update.asks.push_back(level);
        }

        if (updateCallback)
        {
            updateCallback(update);
        }
        else
        {
            std::cout << "\n=== " << sym << "\n";

            std::cout << "Bids\n";
            for (const auto &bid : update.bids)
            {
                std::cout << "Price: " << bid.price << " | Quantity: " << bid.quantity << "\n";
            }

            std::cout << "Asks\n";
            for (const auto &ask : update.asks)
            {
                std::cout << "Price: " << ask.price << " | Quantity: " << ask.quantity << "\n";
            }

            std::cout << "Mid Price: " << update.midPrice() << "\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Parse/print error: " << e.what() << "\n";
    }

    // continue reading after getting first batch
    ws.async_read(buffer, beast::bind_front_handler(&MarketData::onRead, this));
}