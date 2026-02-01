#include "mojo/client.hpp"
#include "mojo/logger.hpp"
#include <curl/curl.h>

namespace Mojo {

namespace {
    size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::string* str = static_cast<std::string*>(userp);
        str->append(static_cast<char*>(contents), total_size);
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
        // Clear proxy if empty string passed? Or assume caller handles logic.
        // libcurl clears proxy if set to empty string.
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
    
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, "Mojo/1.0");
    curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 3L); // Reduced timeout to 3s for speed

    CURLcode res = curl_easy_perform(curl_);
    
    if (res != CURLE_OK) {
        response.success = false;
        response.error = curl_easy_strerror(res);
        // Logger::warn("CURL error: " + response.error); // Let caller decide if warn/error
        return response;
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
