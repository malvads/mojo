#include "mojo/client.hpp"
#include "mojo/logger.hpp"
#include <curl/curl.h>

namespace Mojo {

namespace {
    struct RequestContext {
        std::string* body;
        bool is_image = false;
    };

    size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        RequestContext* ctx = static_cast<RequestContext*>(userp);
        if (ctx->is_image) return 0; // Abort if identified as image

        size_t total_size = size * nmemb;
        ctx->body->append(static_cast<char*>(contents), total_size);
        return total_size;
    }

    size_t header_callback(char* buffer, size_t size, size_t nitems, void* userp) {
        size_t total_size = size * nitems;
        RequestContext* ctx = static_cast<RequestContext*>(userp);
        
        std::string header(buffer, total_size);
        // Normalize to lowercase for checking
        for (char& c : header) c = std::tolower(c);

        if (header.find("content-type:") != std::string::npos) {
            // Check if content-type contains "image/"
            if (header.find("image/") != std::string::npos) {
                ctx->is_image = true;
                return 0; // Signal libcurl to abort transfer
            }
        }
        return total_size;
    }
}

Client::Client() {
    curl_ = curl_easy_init();
    if (!curl_) {
        Logger::error("Failed to initialize CURL handle");
        return;
    }
}

Client::~Client() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

void Client::set_proxy(const std::string& proxy) {
    if (curl_) {
        curl_easy_setopt(curl_, CURLOPT_PROXY, proxy.c_str());
    } else {
        curl_easy_setopt(curl_, CURLOPT_PROXY, ""); 
    }
}

Response Client::get(const std::string& url) {
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
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, "Mojo/1.0");
    curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 3L);

    CURLcode res = curl_easy_perform(curl_);
    
    if (res != CURLE_OK) {
        if (ctx.is_image) {
            response.success = false;
            response.error = "Skipped: Image detected";
            // We can return early, but maybe we want status code if available? 
            // Usually aborted transfer might not have status code set yet or partially.
            // But let's check info anyway.
        } else {
            response.success = false;
            response.error = curl_easy_strerror(res);
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
    
    if (response_code >= 400) {
        response.success = false; // HTTP Error is still a "success" for CURL, but "failure" for scraping usually
        response.error = "HTTP " + std::to_string(response_code);
    } else {
        response.success = true;
    }

    return response;
}

}
