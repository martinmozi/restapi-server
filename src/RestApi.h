#pragma once

#include "include/restapi/IRestApi.h"

namespace libRestApi
{
    class HttpServer;
    class RestApi : public IRestApi
    {
    public:
        RestApi(int port, const std::string& basicUrl);
        void start(HttpHandler&& httpHandler) override;

    protected:
        void stop() override;

    private:
        std::unique_ptr<HttpServer> httpServer_;
        int port_;
        std::string basicUrl_;
    };
}

