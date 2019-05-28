#include <iostream>

#include "include/restapi/IRestApi.h"

int main(int argc, char** argv)
{
    auto iRestApi = libRestApi::createRestApiServer(8080, "/api");
    iRestApi->start([](libRestApi::HttpMethod method, const std::string & url, const std::string & request)->std::string
    {
        // toto uz je automaticky rozvlaknene a ma to vlastny stack, mozno tu pridam id threadu, aby sa dali nejako nad tym indexovat objekty
            return "ff";
    });
    return 1;
}
