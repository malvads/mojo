#include "beast_client.hpp"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "../../binary/reader.hpp"
#include "../../binary/writer.hpp"
#include "../../core/logger/logger.hpp"
#include "../../core/types/constants.hpp"
#include "../../utils/url/url.hpp"
#include "../proxy/socks_handshake.hpp"

namespace Mojo {
namespace Network {
namespace Http {

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;
using tcp       = net::ip::tcp;

BeastClient::BeastClient(net::io_context& /*ioc*/) {
    ssl_ctx_.set_default_verify_paths();
    ssl_ctx_.set_verify_mode(ssl::verify_peer);
}

void BeastClient::set_proxy(const std::string& proxy) {
    proxy_ = proxy;
}

void BeastClient::set_connect_timeout(std::chrono::milliseconds timeout) {
    connect_timeout_ = timeout;
}

net::awaitable<Response> BeastClient::get(const std::string& url) {
    co_return co_await do_request(http::verb::get, url);
}

net::awaitable<Response> BeastClient::head(const std::string& url) {
    co_return co_await do_request(http::verb::head, url);
}

net::awaitable<Response> BeastClient::do_request(http::verb method, const std::string& url) {
    auto parsed = Mojo::Utils::Url::parse(url);
    if (parsed.host.empty()) {
        co_return Response{.effective_url = "",
                           .status_code   = 0,
                           .content_type  = "",
                           .body          = "",
                           .error         = "Invalid URL",
                           .success       = false,
                           .skipped       = false,
                           .error_type    = ErrorType::Network};
    }

    std::string host = parsed.host;
    std::string port =
        parsed.port.empty() ? (parsed.scheme == "https" ? "443" : "80") : parsed.port;
    std::string target = parsed.path.empty() ? "/" : parsed.path;
    if (!parsed.query.empty())
        target += "?" + parsed.query;
    if (!parsed.fragment.empty())
        target += "#" + parsed.fragment;

    bool is_ssl = (parsed.scheme == "https");

    co_return co_await do_request_impl(host, port, target, is_ssl, method);
}

net::awaitable<Response> BeastClient::do_request_impl(const std::string& host,
                                                      const std::string& port,
                                                      const std::string& target,
                                                      bool               is_ssl,
                                                      http::verb         method) {
    std::string effective_url = (is_ssl ? "https://" : "http://") + host + ":" + port + target;

    bool        use_proxy = !proxy_.empty();
    std::string proxy_scheme;
    if (use_proxy) {
        auto proxy_parsed = Mojo::Utils::Url::parse(proxy_);
        proxy_scheme      = proxy_parsed.scheme;
    }

    try {
        if (!is_ssl) {
            co_return co_await perform_http_request(
                host, port, target, method, use_proxy, proxy_scheme, effective_url);
        }
        else {
            co_return co_await perform_https_request(
                host, port, target, method, use_proxy, proxy_scheme);
        }
    } catch (const std::exception& e) {
        Response response;
        response.effective_url = effective_url;
        response.success       = false;
        response.error         = e.what();
        response.error_type    = ErrorType::Network;
        response.status_code   = 0;
        co_return response;
    }
}

net::awaitable<Response> BeastClient::perform_http_request(const std::string& host,
                                                           const std::string& port,
                                                           const std::string& target,
                                                           http::verb         method,
                                                           bool               use_proxy,
                                                           const std::string& proxy_scheme,
                                                           const std::string& effective_url) {
    Response response;
    response.effective_url = effective_url;

    std::string connect_host = host;
    std::string connect_port = port;

    if (use_proxy) {
        auto proxy_parsed = Mojo::Utils::Url::parse(proxy_);
        if (!proxy_parsed.host.empty()) {
            connect_host = proxy_parsed.host;
            connect_port = proxy_parsed.port.empty() ? "8080" : proxy_parsed.port;
        }
    }

    tcp::resolver resolver(co_await net::this_coro::executor);
    auto results = co_await resolver.async_resolve(connect_host, connect_port, net::use_awaitable);

    beast::tcp_stream stream(co_await net::this_coro::executor);
    stream.expires_after(connect_timeout_);
    co_await stream.async_connect(results, net::use_awaitable);

    if (use_proxy) {
        co_await handle_http_proxy_handshake(stream.socket(), host, port);
    }

    stream.expires_after(std::chrono::seconds(Mojo::Core::Constants::REQUEST_TIMEOUT_SECONDS));

    std::string req_target = target;
    if (use_proxy && (proxy_scheme.empty() || proxy_scheme == "http")) {
        req_target = response.effective_url;
    }

    http::request<http::string_body> req{method, req_target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, Mojo::Core::Constants::USER_AGENT);

    co_await http::async_write(stream, req, net::use_awaitable);

    beast::flat_buffer                b;
    http::response<http::string_body> res;
    co_await                          http::async_read(stream, b, res, net::use_awaitable);

    response.status_code = res.result_int();
    response.body        = std::move(res.body());
    response.success     = (response.status_code >= 200 && response.status_code < 400);
    auto ct              = res.find(http::field::content_type);
    if (ct != res.end())
        response.content_type = std::string(ct->value());

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    co_return response;
}

net::awaitable<Response> BeastClient::perform_https_request(const std::string& host,
                                                            const std::string& port,
                                                            const std::string& target,
                                                            http::verb         method,
                                                            bool               use_proxy,
                                                            const std::string& proxy_scheme) {
    Response response;
    response.effective_url = "https://" + host + ":" + port + target;

    std::string connect_host = host;
    std::string connect_port = port;

    if (use_proxy) {
        auto proxy_parsed = Mojo::Utils::Url::parse(proxy_);
        if (!proxy_parsed.host.empty()) {
            connect_host = proxy_parsed.host;
            connect_port = proxy_parsed.port.empty() ? "8080" : proxy_parsed.port;
        }
    }

    tcp::resolver resolver(co_await net::this_coro::executor);
    auto results = co_await resolver.async_resolve(connect_host, connect_port, net::use_awaitable);

    beast::ssl_stream<beast::tcp_stream> ssl_stream(co_await net::this_coro::executor, ssl_ctx_);
    if (!SSL_set_tlsext_host_name(ssl_stream.native_handle(), host.c_str())) {
        throw beast::system_error(
            beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
    }

    beast::get_lowest_layer(ssl_stream).expires_after(connect_timeout_);
    co_await beast::get_lowest_layer(ssl_stream).async_connect(results, net::use_awaitable);

    if (use_proxy) {
        if (proxy_scheme == "socks5") {
            co_await Mojo::Network::Proxy::SocksHandshake::perform_socks5(
                beast::get_lowest_layer(ssl_stream).socket(), host, port);
        }
        else if (proxy_scheme == "socks4") {
            co_await Mojo::Network::Proxy::SocksHandshake::perform_socks4(
                beast::get_lowest_layer(ssl_stream).socket(), host, port);
        }
        else {
            // HTTP CONNECT
            http::request<http::empty_body> req{http::verb::connect, host + ":" + port, 11};
            req.set(http::field::host, host + ":" + port);
            req.set(http::field::user_agent, Mojo::Core::Constants::USER_AGENT);
            co_await http::async_write(
                beast::get_lowest_layer(ssl_stream), req, net::use_awaitable);

            beast::flat_buffer               b;
            http::response<http::empty_body> res;
            co_await                         http::async_read(
                beast::get_lowest_layer(ssl_stream), b, res, net::use_awaitable);

            if (res.result() != http::status::ok) {
                response.status_code = res.result_int();
                response.error       = "Proxy CONNECT failed";
                co_return response;
            }
        }
    }

    beast::get_lowest_layer(ssl_stream).expires_after(connect_timeout_);
    co_await ssl_stream.async_handshake(ssl::stream_base::client, net::use_awaitable);

    beast::get_lowest_layer(ssl_stream)
        .expires_after(std::chrono::seconds(Mojo::Core::Constants::REQUEST_TIMEOUT_SECONDS));

    http::request<http::string_body> req{method, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, Mojo::Core::Constants::USER_AGENT);

    co_await http::async_write(ssl_stream, req, net::use_awaitable);

    beast::flat_buffer                b;
    http::response<http::string_body> res;
    co_await                          http::async_read(ssl_stream, b, res, net::use_awaitable);

    response.status_code = res.result_int();
    response.body        = std::move(res.body());
    response.success     = (response.status_code >= 200 && response.status_code < 400);
    auto ct              = res.find(http::field::content_type);
    if (ct != res.end())
        response.content_type = std::string(ct->value());

    co_await  ssl_stream.async_shutdown(net::use_awaitable);
    co_return response;
}

net::awaitable<void> BeastClient::handle_http_proxy_handshake(net::ip::tcp::socket& socket,
                                                              const std::string&    host,
                                                              const std::string&    port) {
    auto proxy_parsed = Mojo::Utils::Url::parse(proxy_);
    if (proxy_parsed.scheme == "socks5") {
        co_await Mojo::Network::Proxy::SocksHandshake::perform_socks5(socket, host, port);
    }
    else if (proxy_parsed.scheme == "socks4") {
        co_await Mojo::Network::Proxy::SocksHandshake::perform_socks4(socket, host, port);
    }
    co_return;
}

}  // namespace Http
}  // namespace Network
}  // namespace Mojo
