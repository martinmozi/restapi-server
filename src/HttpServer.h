#pragma once

#include "../include/restapi-server/IRestApiServer.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <thread>
#include <mutex>

namespace libRestApi
{
    class HttpServer;
    class listener : public std::enable_shared_from_this<listener>
    {
    public:
        listener(HttpServer& httpServer, boost::asio::io_context& ioc, boost::asio::ssl::context& ctx, boost::asio::ip::tcp::endpoint endpoint);
        void run();

    private:
        void do_accept();
        void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

    private:
        HttpServer & httpServer_;
        boost::asio::io_context& ioc_;
        boost::asio::ssl::context& ctx_;
        boost::asio::ip::tcp::acceptor acceptor_;
    };

    class HttpServer
    {
    public:
        struct HandlerPair
        {
            HttpHandler httpHandler;
            bool locked = false;
        };
    public:
        HttpServer();
        ~HttpServer();
        void start(uint16_t port, HttpHandler httpHandler);
        void stop();
        HandlerPair& httpHandlerAcquire();
        void httpHandlerRelease(HandlerPair &handlerPair);

    private:
        int threads_;
        boost::asio::io_context ioc_;
        std::shared_ptr<listener> listener_;
        std::vector<std::thread> workers_;

        std::mutex mutex_;
        std::vector<HandlerPair> handlers_;
    };
}

