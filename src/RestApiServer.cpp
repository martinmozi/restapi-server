#include "RestApiServer.h"
#include "HttpServer.h"

#ifdef WIN32
std::string tmpDir("C:/Temp");
#else
std::string tmpDir("/tmp");
#endif

namespace libRestApi
{
    RestApiServer::RestApiServer(int port, const std::string& basicUrl)
    :   port_(port),
        basicUrl_(basicUrl)
    {
    }

    void RestApiServer::start(HttpHandler&& httpHandler)
    {
        httpServer_.reset(new HttpServer);
        size_t sz = basicUrl_.size();
        httpServer_->start(port_, [sz, httpHandler](libRestApi::HttpMethod method, const std::string url, std::string && request)->std::string
        {
            std::string _url;
            if (url.size() >= sz)
                std::string _url = url.substr(sz);

            return httpHandler(method, _url, std::move(request));
        });
    }

    void RestApiServer::stop()
    {
        httpServer_.reset(nullptr);
    }

    std::unique_ptr<IRestApiServer> createRestApiServer(int port, const std::string basicUrl)
    {
        return std::make_unique<RestApiServer>(port, basicUrl);
    }
}
