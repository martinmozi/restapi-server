# boost-httpserver
light http server based on boost beast

# usage

```
    auto iRestApi = libRestApi::createRestApiServer(8080, "/api");
    iRestApi->start([](libRestApi::HttpMethod method, const std::string & url, const std::string & request)->std::string
    {
        return "return some string according url and request";
    });
```