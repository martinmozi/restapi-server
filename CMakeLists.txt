cmake_minimum_required(VERSION 3.9)

project(restapi-server VERSION 1.0.0 DESCRIPTION "http server for easy restapi usage")

add_library(restapi-server SHARED
    src/HttpServer.cpp
    src/buffers.cpp
    [...]
)

set_target_properties(restapi-server PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(restapi-server PROPERTIES SOVERSION 1)
set_target_properties(mylib PROPERTIES PUBLIC_HEADER include/restapi/IRestApi.h)