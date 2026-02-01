#pragma once
#include <string>
#include <optional>

typedef void CURL;

namespace Mojo {

struct Response {
    bool success = false;
    long status_code = 0;
    std::string body;
    std::string error;
    std::string effective_url; // Final URL after redirects
    std::string content_type;
};

class Client {
public:
    explicit Client();
    ~Client();
    
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    void set_proxy(const std::string& proxy);
    Response get(const std::string& url);

private:
    CURL* curl_;
};

}
