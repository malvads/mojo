#pragma once
#include <string>
#include <vector>
#include <memory>

namespace Mojo {
    
enum class ErrorType {
    None,
    Network,
    Proxy,
    Timeout,
    Browser,
    Render,
    Skipped,
    Other
};

struct Response {
    bool success = false;
    long status_code = 0;
    std::string body;
    std::string error;
    ErrorType error_type = ErrorType::None;
    std::string effective_url;
    std::string content_type;
};

class HttpClient {
public:
    virtual ~HttpClient() = default;
    
    virtual void set_proxy(const std::string& proxy) = 0;
    virtual Response get(const std::string& url) = 0;
};

}
