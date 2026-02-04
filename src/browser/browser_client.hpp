#pragma once
#include "../network/http/http_client.hpp"

namespace Mojo {
namespace Browser {

using namespace Mojo::Network::Http;

class BrowserClient : public HttpClient {
public:
    explicit BrowserClient(boost::asio::io_context& ioc);
    ~BrowserClient() override = default;

    void                             set_proxy(const std::string& proxy) override;
    boost::asio::awaitable<Response> get(const std::string& url) override;
    boost::asio::awaitable<Response> head(const std::string& url) override;

private:
    boost::asio::io_context&     ioc_;
    std::string                  proxy_;
    boost::asio::awaitable<bool> render_to_response(const std::string& url, Response& res);
};

}  // namespace Browser
}  // namespace Mojo
