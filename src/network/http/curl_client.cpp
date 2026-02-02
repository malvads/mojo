#include "curl_client.hpp"
#include <curl/curl.h>
#include <memory>
#include <string>
#include <string_view>
#include <algorithm>
#include "../../core/types/constants.hpp"

namespace Mojo {
namespace Network {
namespace Http {

namespace {

constexpr std::string_view CONTENT_TYPE_HEADER = "content-type:";
constexpr std::string_view IMAGE_PREFIX = "image/";

static inline std::string_view trim_view(std::string_view s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static bool icontains(std::string_view haystack, std::string_view needle) {
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char c1, char c2) {
            return std::tolower(static_cast<unsigned char>(c1)) == 
                   std::tolower(static_cast<unsigned char>(c2));
        }
    );
    return it != haystack.end();
}

static bool istarts_with(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) return false;
    return std::equal(
        prefix.begin(), prefix.end(),
        text.begin(),
        [](char c1, char c2) {
            return std::tolower(static_cast<unsigned char>(c1)) == 
                   std::tolower(static_cast<unsigned char>(c2));
        }
    );
}

static Network::Http::ErrorType map_curl_code_to_error_type(CURLcode code) {
    switch (code) {
        case CURLE_COULDNT_CONNECT:
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_RECV_ERROR: return Network::Http::ErrorType::Proxy;
        case CURLE_OPERATION_TIMEDOUT: return Network::Http::ErrorType::Timeout;
        default: return Network::Http::ErrorType::Network;
    }
}

}

size_t CurlClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* ctx = static_cast<CurlClient::RequestContext*>(userp);
    if (!ctx || !ctx->body) return 0;
    
    size_t total = size * nmemb;
    if (ctx->is_image && ctx->detect_image) return total;
    ctx->body->append(static_cast<const char*>(contents), total);
    return total;
}

size_t CurlClient::header_callback(char* buffer, size_t size, size_t nitems, void* userp) {
    auto* ctx = static_cast<CurlClient::RequestContext*>(userp);
    if (!ctx || !ctx->content_type) return size * nitems;

    std::string header(buffer, size * nitems);
    
    if (header.size() < CONTENT_TYPE_HEADER.size()) return size * nitems;

    if (!icontains(header, CONTENT_TYPE_HEADER)) {
        return size * nitems;
    }

    auto colon = header.find(':');
    if (colon == std::string::npos) {
        return size * nitems;
    }

    std::string val(trim_view(header.substr(colon + 1)));
    *ctx->content_type = val;

    if (istarts_with(val, IMAGE_PREFIX)) {
        ctx->is_image = true;
    }

    return size * nitems;
}

Response CurlClient::create_error_response(const std::string& msg) const {
    Response r;
    r.success = false;
    r.error = msg;
    r.error_type = Network::Http::ErrorType::Network;
    r.status_code = static_cast<long>(HTTPCode::NetworkError);
    return r;
}

void CurlClient::setup_curl_options(CURL* curl, const Request& req, RequestContext& ctx, const std::string& proxy) const {
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, req.follow_location ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(req.timeout_seconds));

    if (!req.user_agent.empty()) curl_easy_setopt(curl, CURLOPT_USERAGENT, req.user_agent.c_str());
    if (req.method == HttpMethod::HEAD) curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    else curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    if (!proxy.empty()) curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());

    struct CurlSlistDeleter { void operator()(curl_slist* p) const noexcept { curl_slist_free_all(p); } };
    curl_slist* raw = nullptr;
    for (const auto& h : req.extra_headers) raw = curl_slist_append(raw, h.c_str());
    std::unique_ptr<curl_slist, CurlSlistDeleter> header_list(raw);
    if (raw) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, raw);
}

CURLcode CurlClient::perform_curl_request(CURL* curl, const Request& req, RequestContext& ctx, const std::string& proxy, long& out_response_code, std::string& out_effective_url) const {
    setup_curl_options(curl, req, ctx, proxy);
    CURLcode cres = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out_response_code);
    char* eff_url_ptr = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &eff_url_ptr);
    out_effective_url = eff_url_ptr ? std::string(eff_url_ptr) : req.url;
    return cres;
}

Response CurlClient::handle_response(CURLcode res, RequestContext& ctx, long response_code, const std::string& effective_url, std::string& body, std::string& content_type) const {
    Response response;
    response.effective_url = effective_url;
    response.status_code = response_code;
    response.content_type = content_type;

    if (ctx.is_image) {
        response.success = false;
        response.skipped = true;
        response.error = "Skipping image, content-type: " + content_type;
        response.error_type = Network::Http::ErrorType::Skipped;
        response.status_code = static_cast<long>(HTTPCode::Ok);
        return response;
    }

    if (res != CURLE_OK) {
        response.success = false;
        response.error = curl_easy_strerror(res);
        response.error_type = map_curl_code_to_error_type(res);
        response.status_code = static_cast<long>(HTTPCode::NetworkError);
        return response;
    }

    response.body = std::move(body);
    response.success = (response.status_code >= 200 && response.status_code < static_cast<long>(MaxCode::ClientError));
    if (!response.success && response.error.empty()) {
        response.error = "HTTP " + std::to_string(response.status_code);
    }
    return response;
}

CurlClient::Request CurlClient::create_request(const std::string& url, HttpMethod method) {
    Request req;
    req.method = method;
    req.url = url;
    if (method == HttpMethod::HEAD) {
        req.timeout_seconds = 5L;
    }
    return req;
}

CurlClient::CurlClient() : curl_(curl_easy_init()) {}

void CurlClient::set_proxy(const std::string& proxy) { proxy_ = proxy; }

Response CurlClient::perform(const Request& req) {
    if (!curl_) return create_error_response("Failed to initialize CURL handle");

    std::string body_buffer;
    std::string content_type;
    std::string effective_url;
    RequestContext ctx{ &body_buffer, &content_type };

    long response_code = 0;
    CURLcode res = perform_curl_request(curl_.get(), req, ctx, proxy_, response_code, effective_url);
    return handle_response(res, ctx, response_code, effective_url, body_buffer, content_type);
}

Response CurlClient::get(const std::string& url) {
    return perform(create_request(url, HttpMethod::GET));
}

Response CurlClient::head(const std::string& url) {
    return perform(create_request(url, HttpMethod::HEAD));
}

} // namespace Http
} // namespace Network
} // namespace Mojo
