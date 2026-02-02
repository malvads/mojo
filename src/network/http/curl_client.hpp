#pragma once
#include "http_client.hpp"
#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>

namespace Mojo {
namespace Network {
namespace Http {

class CurlClient : public HttpClient {
public:
    enum class HttpMethod { GET, HEAD };

    CurlClient();
    ~CurlClient() = default; 
    CurlClient(const CurlClient&) = delete;
    CurlClient& operator=(const CurlClient&) = delete;

    void set_proxy(const std::string& proxy) override;
    Response get(const std::string& url) override;
    Response head(const std::string& url);

private:
    struct Request {
        HttpMethod method = HttpMethod::GET;
        std::string url;
        long timeout_seconds = 10;
        bool follow_location = true;
        std::vector<std::string> extra_headers;
        std::string user_agent = "mojo/crawler";
    };

    struct RequestContext {
        std::string* body = nullptr;
        std::string* content_type = nullptr;
        bool detect_image = true;
        bool is_image = false;
    };
    
    struct CurlDeleter {
        void operator()(CURL* curl) const noexcept {
            if (curl) curl_easy_cleanup(curl);
        }
    };

    std::unique_ptr<CURL, CurlDeleter> curl_;
    std::string proxy_;

    Response perform(const Request& req);
    
    Response create_error_response(const std::string& msg) const;
    void setup_curl_options(CURL* curl, const Request& req, RequestContext& ctx, const std::string& proxy) const;
    CURLcode perform_curl_request(CURL* curl, const Request& req, RequestContext& ctx, const std::string& proxy, long& out_response_code, std::string& out_effective_url) const;
    Response handle_response(CURLcode res, RequestContext& ctx, long response_code, const std::string& effective_url, std::string& body, std::string& content_type) const;
    Request create_request(const std::string& url, HttpMethod method = HttpMethod::GET);

    // Callbacks must be static. userp is guaranteed to be RequestContext*.
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userp);
};

} // namespace Http
} // namespace Network
} // namespace Mojo