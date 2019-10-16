#pragma once

#include "../include/restapi-server/IRestApiServer.h"
#include <boost/beast/core.hpp>
#include <thread>
#include <mutex>

namespace libRestApi
{
    class HttpServer
    {
    public:
        struct HandlerPair
        {
            HttpHandler httpHandler;
            bool locked = false;
        };
    public:
        HttpServer(const std::string& docRoot);
        ~HttpServer();
        void start(uint16_t port, HttpHandler httpHandler);
        void stop();
        HandlerPair& httpHandlerAcquire();
        void httpHandlerRelease(HandlerPair &handlerPair);

    private:
        int threads_;
        boost::asio::io_context ioc_;
        std::shared_ptr<std::string> docRoot_;
        std::vector<std::thread> workers_;

        std::mutex mutex_;
        std::vector<HandlerPair> handlers_;
    };
}

