#include "mojo/curl_client.hpp"
#include "mojo/logger.hpp"
#include "mojo/constants.hpp"
#include "mojo/statuses.hpp"
#include <curl/curl.h>

namespace Mojo {

namespace {
    struct RequestContext {
        std::string* body;
        std::string content_type;
        bool is_image = false;
    };

    size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        RequestContext* ctx = static_cast<RequestContext*>(userp);
        if (ctx->is_image) return 0;

        size_t total_size = size * nmemb;
        ctx->body->append(static_cast<char*>(contents), total_size);
        return total_size;
    }

    size_t header_callback(char* buffer, size_t size, size_t nitems, void* userp) {
        size_t total_size = size * nitems;
        RequestContext* ctx = static_cast<RequestContext*>(userp);
        
        std::string header(buffer, total_size);
        std::string header_lower = header;
        for (char& c : header_lower) c = std::tolower(c);

        if (header_lower.find("content-type:") != std::string::npos) {
            if (header_lower.find("image/") != std::string::npos) {
                ctx->is_image = true;
                return 0;
            }
            size_t colon = header.find(':');
            if (colon != std::string::npos) {
                 std::string val = header.substr(colon + 1);
                 val.erase(0, val.find_first_not_of(" \t\r\n"));
                 val.erase(val.find_last_not_of(" \t\r\n") + 1);
                 ctx->content_type = val;
            }
        }
        return total_size;
    }
}

CurlClient::CurlClient() {
    curl_ = curl_easy_init();
    if (!curl_) {
        Logger::error("Failed to initialize CURL handle");
    }
}

CurlClient::~CurlClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

void CurlClient::set_proxy(const std::string& proxy) {
    if (curl_) {
        curl_easy_setopt(curl_, CURLOPT_PROXY, proxy.c_str());
    }
}

Response CurlClient::get(const std::string& url) {
    Response response;
    if (!curl_) {
        response.error = "CURL not initialized";
        return response;
    }

    std::string buffer;
    RequestContext ctx;
    ctx.body = &buffer;
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &ctx);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, Constants::USER_AGENT);
    curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, static_cast<long>(Constants::REQUEST_TIMEOUT_SECONDS));


    CURLcode res = curl_easy_perform(curl_);
    
    if (res != CURLE_OK) {
        if (ctx.is_image) {
            response.success = false;
            response.error = "Skipped: Image detected";
            response.status_code = static_cast<long>(Status::Ok); // Treated as OK but skipped
        } else {
            response.success = false;
            response.error = curl_easy_strerror(res);
            response.status_code = static_cast<long>(Status::NetworkError);
            return response;
        }
    }

    long response_code;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
    
    char* effective_url_ptr = nullptr;
    curl_easy_getinfo(curl_, CURLINFO_EFFECTIVE_URL, &effective_url_ptr);
    if (effective_url_ptr) {
        response.effective_url = std::string(effective_url_ptr);
    } else {
        response.effective_url = url;
    }
    
    response.status_code = response_code;
    response.body = std::move(buffer);
    response.content_type = ctx.content_type;
    
    if (response_code >= static_cast<long>(MaxCode::ClientError)) {
        response.success = false;
        response.error = "HTTP " + std::to_string(response_code);
    } else {
        response.success = true;
    }

    return response;
}

Response CurlClient::head(const std::string& url) {
    Response response;
    if (!curl_) {
        response.error = "CURL not initialized";
        return response;
    }

    std::string buffer;
    RequestContext ctx;
    ctx.body = &buffer;

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_NOBODY, 1L); // HEAD request
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &ctx);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, Constants::USER_AGENT);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 5L); // Short timeout for HEAD

    CURLcode res = curl_easy_perform(curl_);

    long response_code;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
    
    response.status_code = response_code;
    response.content_type = ctx.content_type;
    
    if (res != CURLE_OK) {
        response.success = false;
        response.error = curl_easy_strerror(res);
    } else {
        response.success = (response_code >= 200 && response_code < 300);
    }
    
    curl_easy_setopt(curl_, CURLOPT_NOBODY, 0L);
    
    return response;
}

}
