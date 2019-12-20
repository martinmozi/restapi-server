#pragma once

#include <functional>
#include <memory>
#include <string>

namespace libRestApi
{
    enum HttpMethod
    {
        Get,
        Post
    };

    using HttpHandler = std::function<std::string(HttpMethod, const std::string, std::string&&)>;
    class IRestApiServer
    {
    public:
        virtual ~IRestApiServer() { stop(); }
        virtual void start(HttpHandler&& httpHandler) = 0;

    protected:
        virtual void stop() {}
    };

    std::unique_ptr<IRestApiServer> createRestApiServer(int port, const std::string basicUrl);
}