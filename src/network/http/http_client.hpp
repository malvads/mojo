#pragma once
#include <string>
#include <vector>
#include "../../core/types/statuses.hpp"

namespace Mojo {
struct Response {
    std::string effective_url;
    long status_code = 0;
    std::string content_type;
    std::string body;
    std::string error;
    bool success = false;
    ErrorType error_type = ErrorType::None; // Check where ErrorType is defined (statuses.hpp?)
};

namespace Network {
namespace Http {

class HttpClient {
public:
    virtual ~HttpClient() = default;
    
    virtual void set_proxy(const std::string& proxy) = 0;
    virtual Response get(const std::string& url) = 0;
};

}
}
}
