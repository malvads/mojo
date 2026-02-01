#pragma once
#include "mojo/http_client.hpp"

namespace Mojo {

class BrowserClient : public HttpClient {
public:
    explicit BrowserClient();
    ~BrowserClient() override = default;
    
    void set_proxy(const std::string& proxy) override;
    Response get(const std::string& url) override;

private:
    std::string proxy_;
    std::string get_content(const std::string& url);
};

}
