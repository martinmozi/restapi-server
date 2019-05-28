#pragma once

#include <boost/beast/core.hpp>
#include <thread>
#include "include/restapi-server/IRestApiServer.h"
#include "jsonVariant.hpp"

namespace libRestApi
{
    class HttpServer
    {
    public:
        HttpServer(const std::string& docRoot);
        ~HttpServer();
        void start(uint16_t port, HttpHandler httpHandler);
        void stop();
        HttpHandler* httpHandler(std::thread::id threadId);

    private:
        int threads_;
        boost::asio::io_context ioc_;
        std::shared_ptr<std::string> docRoot_;
        std::vector<std::thread> workers_;
        std::map<std::thread::id, HttpHandler> handlers_;
    };
}

