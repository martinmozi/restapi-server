#include "RestApi.h"
#include "jsonVariant.hpp"
#include "HttpServer.h"

#ifdef WIN32
std::string tmpDir("C:/Temp");
#else
std::string tmpDir("/tmp");
#endif

namespace libRestApi
{
    RestApi::RestApi(int port, const std::string& basicUrl)
    :   port_(port),
        basicUrl_(basicUrl)
    {

    }

    void RestApi::start(HttpHandler&& httpHandler)
    {
        httpServer_.reset(new HttpServer("/tmp"));
        std::string basicUrl(basicUrl_);
        httpServer_->start(port_, [basicUrl, httpHandler](libRestApi::HttpMethod method, const std::string & url, const std::string & request)->std::string
        {
            // filter according url
            // some json checks
                return httpHandler(method, url, request);
        });
    }

    void RestApi::stop()
    {
        httpServer_.reset(nullptr);
    }

    std::unique_ptr<IRestApi> createRestApiServer(int port, const std::string& basicUrl)
    {
        return std::make_unique<RestApi>(port, basicUrl);
    }
}
