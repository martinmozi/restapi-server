#include "HttpServer.h"

#include "ServerCertificate.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>


using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>

namespace
{
    // This function produces an HTTP response for the given
    // request. The type of the response object depends on the
    // contents of the request, so the interface requires the
    // caller to pass a generic lambda for receiving the response.
    template<class Body, class Allocator, class Send>
    void handle_request(libRestApi::HttpServer& server, boost::beast::string_view /*doc_root*/, http::request<Body, http::basic_fields<Allocator>> && req, Send && send)
    {
        auto const bad_request = [&req](boost::beast::string_view why)
        {
            http::response<http::string_body> res{ http::status::bad_request, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = why.to_string();
            res.prepare_payload();
            return res;
        };

        auto const ok_request = [&req](boost::beast::string_view why)
        {
            http::response<http::string_body> res{ http::status::ok, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = why.to_string();
            res.prepare_payload();
            return res;
        };

        std::string payload;
        std::string url(req.target());
        http::verb method = req.method();
        libRestApi::HttpMethod httpMethod;
        switch (method)
        {
        case http::verb::get:
            httpMethod = libRestApi::HttpMethod::Get;
            break;

        case http::verb::post:
            {
                payload = req.body();
                httpMethod = libRestApi::HttpMethod::Post;
                auto it = req.find(http::field::content_type);
                if (it == req.end() || it->value() != "application/json")
                    return send(bad_request("Not allowed non json Api"));
            }
            break;

            default:
                return send(bad_request("Unsupported http method"));
        }

        auto & httpHandlerPair = server.httpHandlerAcquire();
        std::string response = (httpHandlerPair.httpHandler)(httpMethod, url, payload);
        server.httpHandlerRelease(httpHandlerPair);
        return send(ok_request(response));
    }

    //------------------------------------------------------------------------------

    // Report a failure
    void fail(boost::system::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << "\n";
    }

    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    template<class Stream>
    struct send_lambda
    {
        Stream& stream_;
        bool& close_;
        boost::system::error_code& ec_;
        boost::asio::yield_context yield_;

        explicit
            send_lambda(Stream& stream, bool& close, boost::system::error_code& ec, boost::asio::yield_context yield)
            : stream_(stream)
            , close_(close)
            , ec_(ec)
            , yield_(yield)
        {
        }

        template<bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // Determine if we should close the connection after
            close_ = msg.need_eof();

            // We need the serializer here because the serializer requires
            // a non-const file_body, and the message oriented version of
            // http::write only works with const messages.
            http::serializer<isRequest, Body, Fields> sr{ msg };
            http::async_write(stream_, sr, yield_[ec_]);
        }
    };

    // Handles an HTTP server connection
    void do_session(tcp::socket & socket, ssl::context & ctx, libRestApi::HttpServer& server, std::shared_ptr<std::string const> const& doc_root, boost::asio::yield_context yield)
    {
        bool close = false;
        boost::system::error_code ec;

        // Construct the stream around the socket
        ssl::stream<tcp::socket&> stream{ socket, ctx };

        // Perform the SSL handshake
        stream.async_handshake(ssl::stream_base::server, yield[ec]);
        if (ec)
            return fail(ec, "handshake");

        // This buffer is required to persist across reads
        boost::beast::flat_buffer buffer;

        // This lambda is used to send messages
        send_lambda<ssl::stream<tcp::socket&>> lambda{ stream, close, ec, yield };

        for (;;)
        {
            // Read a request
            http::request<http::string_body> req;
            http::async_read(stream, buffer, req, yield[ec]);
            if (ec == http::error::end_of_stream)
                break;
            if (ec)
                return fail(ec, "read");

            // Send the response
            handle_request(server, *doc_root, std::move(req), lambda);
            if (ec)
                return fail(ec, "write");
            if (close)
            {
                // This means we should close the connection, usually because
                // the response indicated the "Connection: close" semantic.
                break;
            }
        }

        // Perform the SSL shutdown
        stream.async_shutdown(yield[ec]);
        if (ec)
            return fail(ec, "shutdown");

        // At this point the connection is closed gracefully
    }

    //------------------------------------------------------------------------------

    // Accepts incoming connections and launches the sessions
    void do_listen(boost::asio::io_context & ioc, ssl::context & ctx, tcp::endpoint endpoint, std::shared_ptr<std::string const> const& doc_root, libRestApi::HttpServer & server, boost::asio::yield_context yield)
    {
        boost::system::error_code ec;
        tcp::acceptor acceptor(ioc);
        acceptor.open(endpoint.protocol(), ec);
        if (ec)
            return fail(ec, "open");

        acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
            return fail(ec, "set_option");

        acceptor.bind(endpoint, ec);
        if (ec)
            return fail(ec, "bind");

        acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
            return fail(ec, "listen");

        for (;;)
        {
            tcp::socket socket(ioc);
            acceptor.async_accept(socket, yield[ec]);
            if (ec)
                fail(ec, "accept");
            else
                boost::asio::spawn(acceptor.get_executor(), std::bind(
                    &do_session,
                    std::move(socket),
                    std::ref(ctx),
                    std::ref(server),
                    doc_root,
                    std::placeholders::_1));
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

libRestApi::HttpServer::HttpServer(const std::string& docRoot)
:   threads_(std::thread::hardware_concurrency()),
    ioc_(threads_),
    docRoot_(std::make_shared<std::string>(docRoot))
{
}

libRestApi::HttpServer::~HttpServer()
{
    stop();
}

void libRestApi::HttpServer::start(uint16_t port, HttpHandler httpHandler)
{
    ssl::context ctx{ ssl::context::tlsv12 };
    load_server_certificate(ctx);

    boost::asio::spawn(ioc_, std::bind(&do_listen, std::ref(ioc_), std::ref(ctx), tcp::endpoint{ boost::asio::ip::make_address("0.0.0.0"), port }, docRoot_, std::ref(*this), std::placeholders::_1));
    workers_.reserve(threads_ - 1);
    for (auto i = threads_ - 1; i > 0; --i)
    {
        workers_.emplace_back([this] 
        {
            ioc_.run(); 
        });
    }

    for (int i = 0; i < threads_; i++)
    {
        libRestApi::HttpServer::HandlerPair p;
        p.httpHandler = httpHandler;
        p.locked = false;
        handlers_.push_back(std::move(p));
    }

    ioc_.run();
}

void libRestApi::HttpServer::stop()
{
    for (auto i = threads_ - 1; i > 0; --i)
        ioc_.stop();

    for (auto i = threads_ - 1; i > 0; --i)
        workers_.at(i).join();

    ioc_.stop();
    handlers_.clear();
    workers_.clear();
}

libRestApi::HttpServer::HandlerPair& libRestApi::HttpServer::httpHandlerAcquire()
{
    std::lock_guard lock(mutex_);

    while (true)
    {
        for (auto & handlerPair : handlers_)
        {
            if (!handlerPair.locked)
            {
                handlerPair.locked = true;
                return handlerPair;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void libRestApi::HttpServer::httpHandlerRelease(libRestApi::HttpServer::HandlerPair& handlerPair)
{
    std::lock_guard lock(mutex_);
    handlerPair.locked = false;
}
