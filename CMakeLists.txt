cmake_minimum_required(VERSION 3.10)

project(restapi-server VERSION 1.0.0 DESCRIPTION "http server for easy restapi usage")

set (CMAKE_CXX_STANDARD 14)
include(CheckIncludeFileCXX) # Checking existence of an include file
include(GNUInstallDirs) # Installation directories for `install` command and pkgconfig file
include_directories(.)

find_package(OpenSSL REQUIRED)
set(Boost_NO_BOOST_CMAKE TRUE CACHE BOOL "" FORCE)
set(BOOST_MIN_VERSION "1.70.0")
find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS system coroutine REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_definitions(-D__RAPID_JSON_BACKEND)

add_library(restapi-server SHARED
    src/HttpServer.cpp
    src/RestApiServer.cpp
)

add_executable(restapi-server-app
    src/HttpServer.cpp
    src/RestApiServer.cpp
	src/main.cpp
)

target_link_libraries(restapi-server LINK_PUBLIC ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES})

set_target_properties(restapi-server PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(restapi-server PROPERTIES SOVERSION 1)
set_target_properties(restapi-server PROPERTIES PUBLIC_HEADER include/restapi-server/IRestApiServer.h)

install(TARGETS restapi-server
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT libraries # This is for Windows
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT static_libraries
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/restapi-server COMPONENT dev)

# cmake configuration
if(WIN32 AND NOT CYGWIN)
    set(RESTAPI_SERVER_CONFIG_INSTALL_DIR CMake)
else()
    set(RESTAPI_SERVER_CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/RestApiServer)
endif()

include(CMakePackageConfigHelpers)
configure_file(RestApiServerConfig.cmake.in RestApiServerConfig.cmake @ONLY)
write_basic_package_version_file(RestApiServerConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES ${CMAKE_BINARY_DIR}/RestApiServerConfig.cmake
              ${CMAKE_BINARY_DIR}/RestApiServerConfigVersion.cmake DESTINATION ${RESTAPI_SERVER_CONFIG_INSTALL_DIR} COMPONENT dev)


