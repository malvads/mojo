#pragma once
#include <memory>
#include <string>
#include <vector>

namespace Mojo {

struct Response {
    bool        success     = false;
    long        status_code = 0;
    std::string body;
    std::string error;
    ErrorType   error_type = ErrorType::None;
    std::string effective_url;
    std::string content_type;
};

class HttpClient {
public:
    virtual ~HttpClient() = default;

    virtual void     set_proxy(const std::string& proxy) = 0;
    virtual Response get(const std::string& url)         = 0;
};

}  // namespace Mojo
