#pragma once
#include "mojo/http_client.hpp"

typedef void CURL;

namespace Mojo {

class CurlClient : public HttpClient {
public:
    explicit CurlClient();
    ~CurlClient();
    
    CurlClient(const CurlClient&) = delete;
    CurlClient& operator=(const CurlClient&) = delete;
    
    void set_proxy(const std::string& proxy) override;
    Response get(const std::string& url) override;

private:
    CURL* curl_;
};

}
