#pragma once

#include "../include/restapi-server/IRestApiServer.h"

namespace libRestApi
{
    class HttpServer;
    class RestApiServer : public IRestApiServer
    {
    public:
        RestApiServer(int port, const std::string& basicUrl);
        void start(HttpHandler&& httpHandler) override;

    protected:
        void stop() override;

    private:
        std::unique_ptr<HttpServer> httpServer_;
        int port_;
        std::string basicUrl_;
    };
}

