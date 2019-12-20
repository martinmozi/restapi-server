#include "HttpServer.h"
#include "ServerCertificate.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>


using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>

namespace
{
    void fail(boost::beast::error_code ec, char const* what)
    {
        if (ec == boost::asio::ssl::error::stream_truncated)
            return;

        std::cerr << what << ": " << ec.message() << "\n";
    }

    class session : public std::enable_shared_from_this<session>
    {
        // This is the C++11 equivalent of a generic lambda.
        // The function object is used to send an HTTP message.
        struct send_lambda
        {
            session& self_;

            explicit
                send_lambda(session& self)
                : self_(self)
            {
            }

            template<bool isRequest, class Body, class Fields>
            void operator()(http::message<isRequest, Body, Fields>&& msg) const
            {

                auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(std::move(msg));
                self_.res_ = sp;

                // Write the response
                http::async_write(self_.stream_, *sp, boost::beast::bind_front_handler(&session::on_write, self_.shared_from_this(), sp->need_eof()));
            }
        };

        libRestApi::HttpServer& httpServer_;
        boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
        boost::beast::flat_buffer buffer_;
        http::request<http::string_body> req_;
        std::shared_ptr<void> res_;
        send_lambda lambda_;

    public:
        explicit session(libRestApi::HttpServer& httpServer, boost::asio::ip::tcp::socket&& socket, ssl::context& ctx)
        :   httpServer_(httpServer),
            stream_(std::move(socket), ctx),
            lambda_(*this)
        {
            buffer_.reserve(1 * 1024 * 1024); // default 1 Mb buffer
        }

        void run()
        {
            boost::asio::dispatch(stream_.get_executor(), boost::beast::bind_front_handler(&session::on_run, shared_from_this()));
        }

        void on_run()
        {
            boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
            stream_.async_handshake(ssl::stream_base::server, boost::beast::bind_front_handler(&session::on_handshake, shared_from_this()));
        }

        void on_handshake(boost::beast::error_code ec)
        {
            if (ec)
                return fail(ec, "handshake");

            do_read();
        }

        void do_read()
        {
            req_ = {};
            boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
            http::async_read(stream_, buffer_, req_, boost::beast::bind_front_handler(&session::on_read, shared_from_this()));
        }

        void on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);
            if (ec == http::error::end_of_stream)
                return do_close();

            if (ec)
                return fail(ec, "read");

            handle_request(std::move(req_));
        }

        void on_write(bool close, boost::beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            if (ec)
                return fail(ec, "write");

            if (close)
                return do_close();

            res_ = nullptr;
            do_read();
        }

        void do_close()
        {
            boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
            stream_.async_shutdown(boost::beast::bind_front_handler(&session::on_shutdown,shared_from_this()));
        }

        void on_shutdown(boost::beast::error_code ec)
        {
            if (ec)
                return fail(ec, "shutdown");
        }

    private:
        template<class Body, class Allocator>
        void handle_request(http::request<Body, http::basic_fields<Allocator>>&& req)
        {
            auto const bad_request = [&req](boost::beast::string_view why)
            {
                http::response<http::string_body> res{ http::status::bad_request, req.version() };
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = std::string(why);
                res.prepare_payload();
                return res;
            };

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
                httpMethod = libRestApi::HttpMethod::Post;
                auto it = req.find(http::field::content_type);
                if (it == req.end() || it->value() != "application/json")
                    return lambda_(bad_request("Not allowed non json Api"));
            }
            break;

            default:
                return lambda_(bad_request("Unsupported http method"));
            }

            auto& httpHandlerPair = httpServer_.httpHandlerAcquire();
            std::string response = (httpHandlerPair.httpHandler)(httpMethod, url, std::move(req.body()));
            httpServer_.httpHandlerRelease(httpHandlerPair);

            http::response<http::string_body> res{ http::status::ok, req.version() };
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.content_length(response.size());
            res.body() = response;
            return lambda_(std::move(res));
        }
    };
}

libRestApi::listener::listener(libRestApi::HttpServer& httpServer, boost::asio::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint)
:   httpServer_(httpServer),
    ioc_(ioc),
    ctx_(ctx),
    acceptor_(ioc)
{
    boost::beast::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec)
    {
        fail(ec, "open");
        return;
    }

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
        fail(ec, "set_option");
        return;
    }

    acceptor_.bind(endpoint, ec);
    if (ec)
    {
        fail(ec, "bind");
        return;
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        fail(ec, "listen");
        return;
    }
}

void libRestApi::listener::run()
{
    do_accept();
}

void libRestApi::listener::do_accept()
{
    acceptor_.async_accept(boost::asio::make_strand(ioc_), boost::beast::bind_front_handler(&listener::on_accept, shared_from_this()));
}

void libRestApi::listener::on_accept(boost::beast::error_code ec, tcp::socket socket)
{
    if (ec)
        fail(ec, "accept");
    else
        std::make_shared<session>(httpServer_, std::move(socket), ctx_)->run();

    do_accept();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

libRestApi::HttpServer::HttpServer()
:   threads_(std::thread::hardware_concurrency()),
    ioc_(threads_)
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

    for (int i = 0; i < threads_; i++)
    {
        libRestApi::HttpServer::HandlerPair p;
        p.httpHandler = httpHandler;
        p.locked = false;
        handlers_.push_back(std::move(p));
    }

    listener_ = std::make_shared<listener>(*this, ioc_, ctx, boost::asio::ip::tcp::endpoint{ boost::asio::ip::make_address("0.0.0.0"), port });
    listener_->run();

    workers_.reserve(threads_ - 1);
    for (auto i = threads_ - 1; i > 0; --i)
    {
        workers_.emplace_back([this] 
        {
            ioc_.run(); 
        });
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
    std::lock_guard<std::mutex> lock(mutex_);

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
    std::lock_guard<std::mutex> lock(mutex_);
    handlerPair.locked = false;
}
