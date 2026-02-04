#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <string>
#include <utility>
#include "http_client.hpp"

namespace Mojo {
namespace Network {
namespace Http {

class BeastClient : public HttpClient {
public:
    explicit BeastClient(boost::asio::io_context& ioc);
    ~BeastClient() override = default;

    void set_proxy(const std::string& proxy) override;
    void set_connect_timeout(std::chrono::milliseconds timeout) override;
    boost::asio::awaitable<Response> get(const std::string& url) override;
    boost::asio::awaitable<Response> head(const std::string& url) override;

private:
    std::string               proxy_;
    std::chrono::milliseconds connect_timeout_{5000};
    boost::asio::ssl::context ssl_ctx_{boost::asio::ssl::context::tlsv12_client};

    boost::asio::awaitable<Response> do_request(boost::beast::http::verb method,
                                                const std::string&       url);
    boost::asio::awaitable<Response> do_request_impl(const std::string&       host,
                                                     const std::string&       port,
                                                     const std::string&       target,
                                                     bool                     is_ssl,
                                                     boost::beast::http::verb method);

    boost::asio::awaitable<Response> perform_http_request(const std::string&       host,
                                                          const std::string&       port,
                                                          const std::string&       target,
                                                          boost::beast::http::verb method,
                                                          bool                     use_proxy,
                                                          const std::string&       proxy_scheme,
                                                          const std::string&       effective_url);
    boost::asio::awaitable<Response> perform_https_request(const std::string&       host,
                                                           const std::string&       port,
                                                           const std::string&       target,
                                                           boost::beast::http::verb method,
                                                           bool                     use_proxy,
                                                           const std::string&       proxy_scheme);

    boost::asio::awaitable<void> handle_http_proxy_handshake(boost::asio::ip::tcp::socket& socket,
                                                             const std::string&            host,
                                                             const std::string&            port);
};

}  // namespace Http
}  // namespace Network
}  // namespace Mojo
