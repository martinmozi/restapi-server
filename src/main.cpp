#include <iostream>

#include "../include/restapi-server/IRestApiServer.h"

int main(int argc, char** argv)
{
    auto iRestApi = libRestApi::createRestApiServer(8080, "/api");
    iRestApi->start([](libRestApi::HttpMethod method, const std::string & url, const std::string & request)->std::string
    {
		// this is isolated in thread	
        return "{\"someKey\": \"This is json encoded response\"}";
    });
    return 1;
}
