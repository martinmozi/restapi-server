cmake_minimum_required(VERSION 3.9)

project(restapi-server VERSION 1.0.0 DESCRIPTION "http server for easy restapi usage")

include(CheckIncludeFileCXX) # Checking existence of an include file
include(GNUInstallDirs) # Installation directories for `install` command and pkgconfig file

find_package(Boost COMPONENTS beast REQUIRED)

add_library(restapi-server SHARED
    src/HttpServer.cpp
    src/RestApi.cpp
)

set_target_properties(restapi-server PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(restapi-server PROPERTIES SOVERSION 1)
set_target_properties(mylib PROPERTIES PUBLIC_HEADER include/restapi/IRestApi.h)
