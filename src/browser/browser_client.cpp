#include "browser_client.hpp"
#include "browser.hpp"
#include "page.hpp"
#include "../core/logger/logger.hpp"
#include "../network/http/curl_client.hpp"
#include "../core/types/constants.hpp"
#include <algorithm>

namespace Mojo {
namespace Browser {

using namespace Mojo::Core;
using Mojo::Network::Http::CurlClient;

BrowserClient::BrowserClient() {
    Logger::info("Initializing Chromium CDP Browser Engine...");
}

void BrowserClient::set_proxy(const std::string& proxy) {
    proxy_ = proxy;
}

bool BrowserClient::render_to_response(const std::string& url, Response& res) {
    if (url.find("http") != 0) {
        res.error = "Invalid URL scheme";
        res.error_type = Network::Http::ErrorType::Other;
        return false;
    }

    try {
        auto browser = Browser::connect(Browser::kDefaultHost, Browser::kDefaultPort);
        auto page = browser->new_page();
        
        Logger::info("Browser: Navigating to " + url);
        if (!page->goto_url(url)) {
            res.error = "Browser navigation failed (Timeout or Network)";
            res.error_type = Network::Http::ErrorType::Network;
            return false;
        }
        
        res.body = page->content();
        if (res.body.empty()) {
            res.error = "Browser returned empty content";
            res.error_type = Network::Http::ErrorType::Render;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        res.error = std::string("Browser Engine Error: ") + e.what();
        res.error_type = Network::Http::ErrorType::Browser;
        return false;
    } catch (...) {
        res.error = "Unknown Browser Error";
        res.error_type = Network::Http::ErrorType::Browser;
        return false;
    }
}

Response BrowserClient::get(const std::string& url) {
    {
        CurlClient curl;
        curl.set_proxy(proxy_);
        Response head_res = curl.head(url);
        if (head_res.success && Mojo::Core::is_downloadable_mime(head_res.content_type)) {
             Logger::info("Browser: Binary/Document detected (" + head_res.content_type + "), downloading via Curl...");
             return curl.get(url);
        }
    }

    Response res;
    res.success = render_to_response(url, res);
    
    if (res.success) {
        res.status_code = static_cast<long>(HTTPCode::Ok);
    } else {
        if (res.status_code == 0) res.status_code = static_cast<long>(HTTPCode::BrowserError);
        Logger::error("Browser Error [" + url + "]: " + res.error);
    }

    return res;
}

}
}
