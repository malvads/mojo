#pragma once

#include <boost/asio.hpp>
#include <string>
#include <vector>

namespace Mojo {
namespace Network {
namespace Http {

enum class ErrorType { None, Network, Proxy, Timeout, Skipped, Other, Render, Browser };

enum class HTTPCode { Ok = 200, NetworkError = 0, BrowserError = 599, NotFound = 404 };

enum class MaxCode { ClientError = 400 };

}  // namespace Http
}  // namespace Network
}  // namespace Mojo

namespace Mojo {

struct Response {
    std::string              effective_url;
    long                     status_code = 0;
    std::string              content_type;
    std::string              body;
    std::string              error;
    bool                     success    = false;
    bool                     skipped    = false;
    Network::Http::ErrorType error_type = Network::Http::ErrorType::None;
};

namespace Network {
namespace Http {

class HttpClient {
public:
    virtual ~HttpClient() = default;

    virtual void set_proxy(const std::string& proxy) = 0;
    virtual void set_connect_timeout(std::chrono::milliseconds /*timeout*/){};
    virtual boost::asio::awaitable<Response> get(const std::string& url)  = 0;
    virtual boost::asio::awaitable<Response> head(const std::string& url) = 0;
};

}  // namespace Http
}  // namespace Network
}  // namespace Mojo
